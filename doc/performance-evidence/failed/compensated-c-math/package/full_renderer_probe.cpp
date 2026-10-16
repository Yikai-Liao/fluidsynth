#define main compensated_c_quality_screen_main
#include "compensated_c_sf2_screen.cpp"
#undef main

#include "fluid_synth.h"
#include "fluid_voice.h"
#include "fluid_rvoice.h"
#include "fluid_defsfont.h"

#include <array>
#include <memory>
#include <numeric>
#include <time.h>
#include <cstring>

static double monotonic_ns()
{
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<double>(ts.tv_sec) * 1e9 + static_cast<double>(ts.tv_nsec);
}

static size_t nearest_rank(double *values, size_t count, double p)
{
    return std::min(count - 1, static_cast<size_t>(std::ceil(p * count)) - 1);
}

int main(int argc, char **argv)
{
    if(argc != 8)
    {
        std::cerr << "usage: full_renderer_probe <sf2> <cubic|sinc25|compensated6> <blocks> <analysis-dir> <latency.csv> <program> <effects:0|1>\n";
        return 2;
    }
    const std::string method = argv[2];
    const size_t blocks = static_cast<size_t>(std::stoull(argv[3]));
    const int effects = std::stoi(argv[7]);
    constexpr int block_frames = 64;
    constexpr int voices_requested = 16;
    const std::string analysis_dir = argv[4];
    const std::vector<float> prefilter = read_prefilter(analysis_dir + "/prefilter_coefficients.csv", 6);
    const auto half_kernel = make_half_phase_kernel();

    fluid_settings_t *settings = new_fluid_settings();
    if(settings == nullptr) return 2;
    fluid_settings_setnum(settings, "synth.sample-rate", 44100.0);
    fluid_settings_setnum(settings, "synth.gain", 0.2);
    fluid_settings_setint(settings, "synth.polyphony", 256);
    fluid_settings_setint(settings, "synth.cpu-cores", 1);
    fluid_settings_setint(settings, "synth.reverb.active", effects != 0);
    fluid_settings_setint(settings, "synth.chorus.active", effects != 0);
    fluid_settings_setint(settings, "audio.realtime-prio", 0);
    fluid_settings_setint(settings, "synth.lock-memory", 0);

    fluid_synth_t *synth = new_fluid_synth(settings);
    if(synth == nullptr) return 2;
    const double load_begin = monotonic_ns();
    const int sfid = fluid_synth_sfload(synth, argv[1], 1);
    const double load_end = monotonic_ns();
    if(sfid == FLUID_FAILED)
    {
        std::cerr << "failed to load SF2\n";
        return 2;
    }
    const int program = std::stoi(argv[6]);
    if(fluid_synth_program_select(synth, 0, sfid, 0, program) == FLUID_FAILED)
    {
        std::cerr << "failed to select bank 0 / requested program\n";
        return 2;
    }

    const int interp = method == "cubic" ? FLUID_INTERP_4THORDER
        : method == "sinc25" ? FLUID_INTERP_HIGHEST
        : method == "compensated6" ? FLUID_INTERP_COMPENSATED_6PT_EXPERIMENT : -1;
    if(interp < 0 || fluid_synth_set_interp_method(synth, 0, interp) == FLUID_FAILED)
    {
        std::cerr << "invalid interpolation method\n";
        return 2;
    }
    for(int i = 0; i < voices_requested; ++i)
        if(fluid_synth_noteon(synth, 0, 60, 100) == FLUID_FAILED)
            return 2;

    // The synth queues note-on events; process them before looking up their samples.
    std::array<float, block_frames> left{}, right{};
    for(int i = 0; i < 4; ++i)
        if(fluid_synth_write_float(synth, block_frames, left.data(), 0, 1, right.data(), 0, 1) == FLUID_FAILED)
            return 2;

    std::vector<std::unique_ptr<Prepared>> prepared;
    std::unordered_map<fluid_sample_t *, Prepared *> by_sample;
    size_t active_voices = 0;
    size_t active_24bit = 0;
    const double prepare_begin = monotonic_ns();
    if(method == "compensated6")
    {
        for(int i = 0; i < synth->nvoice; ++i)
        {
            fluid_voice_t *voice = synth->voice[i];
            if(voice == nullptr || voice->status != FLUID_VOICE_ON || voice->rvoice == nullptr) continue;
            auto *sample = voice->rvoice->dsp.sample;
            if(sample == nullptr) continue;
            if(sample->data24 != nullptr) ++active_24bit;
            auto found = by_sample.find(sample);
            if(found == by_sample.end())
            {
                if(sample->data24 != nullptr)
                {
                    std::cerr << "selected piano zone uses 24-bit PCM; candidate screen expects int16\n";
                    return 3;
                }
                const size_t frames = static_cast<size_t>(sample->end - sample->start + 1);
                Meta meta;
                meta.label.assign(sample->name, strnlen(sample->name, sizeof(sample->name)));
                meta.raw_file = "in-memory";
                meta.frames = frames;
                meta.loop_start = sample->loopstart >= sample->start ? sample->loopstart - sample->start : 0;
                meta.loop_end = sample->loopend >= sample->start ? sample->loopend - sample->start : 0;
                meta.samplemode = voice->rvoice->dsp.samplemode != FLUID_UNLOOPED ? 1 : 0;
                meta.base_step = 1.0;
                std::vector<double> pcm(frames);
                for(size_t j = 0; j < frames; ++j)
                    pcm[j] = static_cast<double>(sample->data[sample->start + j]) / 32768.0;
                auto item = std::make_unique<Prepared>(prepare_with_pcm(meta, std::move(pcm), half_kernel, prefilter));
                item->pcm.clear();
                item->pcm.shrink_to_fit();
                Prepared *ptr = item.get();
                prepared.push_back(std::move(item));
                by_sample.emplace(sample, ptr);
                found = by_sample.find(sample);
            }
            Prepared *item = found->second;
            auto &dsp = voice->rvoice->dsp;
            dsp.compensated_finite = item->full_finite.data();
            dsp.compensated_attack = item->full_attack.data();
            dsp.compensated_loop = item->periodic_loop.empty() ? nullptr : item->periodic_loop.data();
            dsp.compensated_start = sample->start;
            dsp.compensated_end = sample->end;
            dsp.compensated_loopstart = sample->loopstart;
            dsp.compensated_loopend = sample->loopend;
        }
        if(prepared.empty())
        {
            std::cerr << "no candidate sample tables were prepared; refusing a cubic fallback timing\n";
            return 3;
        }
    }
    const double prepare_end = monotonic_ns();
    for(int i = 0; i < synth->nvoice; ++i)
    {
        fluid_voice_t *voice = synth->voice[i];
        if(voice != nullptr && voice->status == FLUID_VOICE_ON && voice->rvoice != nullptr)
            ++active_voices;
    }
    // Warm up the fixed-size renderer before collecting callback latency.
    for(int i = 0; i < 256; ++i)
        if(fluid_synth_write_float(synth, block_frames, left.data(), 0, 1, right.data(), 0, 1) == FLUID_FAILED)
            return 2;

    std::vector<double> latency_ns(blocks);
    uint64_t audio_hash = UINT64_C(14695981039346656037);
    const double render_begin = monotonic_ns();
    for(size_t b = 0; b < blocks; ++b)
    {
        const double begin = monotonic_ns();
        if(fluid_synth_write_float(synth, block_frames, left.data(), 0, 1, right.data(), 0, 1) == FLUID_FAILED)
            return 2;
        latency_ns[b] = monotonic_ns() - begin;
        for(int i = 0; i < block_frames; ++i)
        {
            uint32_t l, r;
            std::memcpy(&l, &left[static_cast<size_t>(i)], sizeof(l));
            std::memcpy(&r, &right[static_cast<size_t>(i)], sizeof(r));
            for(unsigned shift = 0; shift < 32; shift += 8)
            {
                audio_hash = (audio_hash ^ ((l >> shift) & 0xffu)) * UINT64_C(1099511628211);
                audio_hash = (audio_hash ^ ((r >> shift) & 0xffu)) * UINT64_C(1099511628211);
            }
        }
    }
    const double render_end = monotonic_ns();
    auto sorted = latency_ns;
    std::sort(sorted.begin(), sorted.end());
    const size_t p50 = nearest_rank(sorted.data(), sorted.size(), .50);
    const size_t p99 = nearest_rank(sorted.data(), sorted.size(), .99);
    const size_t p999 = nearest_rank(sorted.data(), sorted.size(), .999);
    const double max_latency = sorted.back();

    std::ofstream csv(argv[5]);
    csv << "block,latency_us\n" << std::setprecision(10);
    for(size_t i = 0; i < latency_ns.size(); ++i) csv << i << ',' << latency_ns[i] / 1000.0 << '\n';
    size_t source_pcm_bytes = 0, candidate_table_bytes = 0;
    for(const auto &entry : by_sample)
    {
        const auto *sample = entry.first;
        source_pcm_bytes += static_cast<size_t>(sample->end - sample->start + 1) * sizeof(short);
        const Prepared *p = entry.second;
        candidate_table_bytes += (p->full_finite.size() + p->full_attack.size() + p->periodic_loop.size()) * sizeof(float);
    }
    double audio_square_sum = 0.0, audio_peak = 0.0;
    size_t audio_nonfinite = 0;
    // The final output buffer contains the last measured callback.
    for(int i = 0; i < block_frames; ++i)
    {
        for(float sample : {left[static_cast<size_t>(i)], right[static_cast<size_t>(i)]})
        {
            if(!std::isfinite(sample)) ++audio_nonfinite;
            audio_square_sum += static_cast<double>(sample) * sample;
            audio_peak = std::max(audio_peak, std::abs(static_cast<double>(sample)));
        }
    }
    std::ofstream prep_csv(std::string(argv[5]) + ".prepare.csv");
    prep_csv << "sample,frames,loop_frames,prepare_ms,table_bytes,original_pcm_bytes\n" << std::setprecision(12);
    for(const auto &entry : by_sample)
    {
        const auto *sample = entry.first;
        const Prepared *p = entry.second;
        const size_t bytes = (p->full_finite.size() + p->full_attack.size() + p->periodic_loop.size()) * sizeof(float);
        prep_csv << p->meta.label << ',' << p->meta.frames << ','
                 << (has_usable_loop(p->meta) ? p->meta.loop_end - p->meta.loop_start : 0) << ','
                 << p->prepare_ms << ',' << bytes << ',' << p->meta.frames * sizeof(short) << '\n';
    }
    std::cout << std::setprecision(8)
              << "{\"method\":\"" << method << "\",\"frames\":" << blocks * block_frames
              << ",\"program\":" << program
              << ",\"effects\":" << effects
              << ",\"blocks\":" << blocks << ",\"block_frames\":" << block_frames
              << ",\"requested_voices\":" << voices_requested << ",\"active_voices\":" << active_voices
              << ",\"unique_prepared_samples\":" << prepared.size() << ",\"active_24bit_samples\":" << active_24bit
              << ",\"sfload_ms\":" << (load_end - load_begin) / 1e6
              << ",\"candidate_prepare_ms\":" << (prepare_end - prepare_begin) / 1e6
              << ",\"candidate_table_bytes\":" << candidate_table_bytes
              << ",\"source_pcm_bytes\":" << source_pcm_bytes
              << ",\"combined_storage_ratio\":" << (source_pcm_bytes ? static_cast<double>(source_pcm_bytes + candidate_table_bytes) / source_pcm_bytes : 1.0)
              << ",\"render_wall_ms\":" << (render_end - render_begin) / 1e6
              << ",\"p50_us\":" << sorted[p50] / 1000.0
              << ",\"p99_us\":" << sorted[p99] / 1000.0
              << ",\"p999_us\":" << sorted[p999] / 1000.0
              << ",\"max_us\":" << max_latency / 1000.0
              << ",\"deadline_us\":" << (1e6 * block_frames / 44100.0)
              << ",\"output_rms\":" << std::sqrt(audio_square_sum / (2.0 * block_frames))
              << ",\"output_peak\":" << audio_peak << ",\"output_nonfinite\":" << audio_nonfinite
              << ",\"audio_hash\":\"" << std::hex << audio_hash << "\"}\n";
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    return 0;
}
