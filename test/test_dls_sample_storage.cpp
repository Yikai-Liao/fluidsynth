#include "test.h"
#include <fluidsynth.h>
#include <cstdint>
#include "fluid_sfont.h"
#include "fluid_voice.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <vector>

using Bytes = std::vector<uint8_t>;
static const unsigned int frame_counts[] = {257, 521, 1031, 2053, 4099, 8209, 16411};
static constexpr unsigned int wave_count = sizeof(frame_counts) / sizeof(frame_counts[0]);

/* Encode independent RIFF fixtures without using production parser helpers. */
static void put16(Bytes &out, uint16_t value)
{
    out.push_back(static_cast<uint8_t>(value));
    out.push_back(static_cast<uint8_t>(value >> 8));
}

static void put32(Bytes &out, uint32_t value)
{
    put16(out, static_cast<uint16_t>(value));
    put16(out, static_cast<uint16_t>(value >> 16));
}

static void append(Bytes &out, const Bytes &part)
{
    out.insert(out.end(), part.begin(), part.end());
}

/* RIFF chunk size excludes its padding, including odd-length 8-bit PCM. */
static Bytes chunk(const char *id, const Bytes &payload)
{
    Bytes out(id, id + 4);
    put32(out, static_cast<uint32_t>(payload.size()));
    append(out, payload);
    if(payload.size() & 1u)
    {
        out.push_back(0);
    }
    return out;
}

static Bytes list(const char *type, const Bytes &children)
{
    Bytes payload(type, type + 4);
    append(payload, children);
    return chunk("LIST", payload);
}

/* Distinct periodic signals include both unsigned PCM endpoints. The 16-bit
 * signals also contain low-byte detail, so byte swapping/truncation is visible. */
static int16_t expected_frame(unsigned int wave, unsigned int frame, unsigned int version)
{
    const int phase = static_cast<int>((frame * (wave + 1) + version * 37) % 256);
    const int pcm = (phase - 128) * 256;
    return static_cast<int16_t>(pcm + ((wave & 1u) ? static_cast<int>(frame % 251) : 0));
}

/* A legal mono wave with wave-level unity note and no loop. */
static Bytes make_wave(unsigned int wave, unsigned int version)
{
    const unsigned int bits = (wave & 1u) ? 16 : 8;
    Bytes format;
    put16(format, 1); // WAVE_FORMAT_PCM
    put16(format, 1);
    put32(format, 44100);
    put32(format, 44100 * bits / 8);
    put16(format, bits / 8);
    put16(format, bits);
    Bytes wsmp;
    put32(wsmp, 20);
    put16(wsmp, 60 + wave);
    put16(wsmp, 0);
    put32(wsmp, 0);
    put32(wsmp, 0);
    put32(wsmp, 0);
    Bytes pcm;
    for(unsigned int frame = 0; frame < frame_counts[wave]; ++frame)
    {
        const int16_t value = expected_frame(wave, frame, version);
        if(bits == 8)
        {
            pcm.push_back(static_cast<uint8_t>(value / 256 + 128));
        }
        else
        {
            put16(pcm, static_cast<uint16_t>(value));
        }
    }
    Bytes children = chunk("fmt ", format);
    append(children, chunk("wsmp", wsmp));
    append(children, chunk("data", pcm));
    return list("wave", children);
}

/* Disjoint keys expose each pool entry individually, including earlier waves
 * whose storage may have moved while later waves were appended. */
static Bytes make_region(unsigned int wave)
{
    Bytes rgnh;
    put16(rgnh, 60 + wave);
    put16(rgnh, 60 + wave);
    put16(rgnh, 0);
    put16(rgnh, 127);
    put16(rgnh, 0);
    put16(rgnh, 0);
    Bytes wlnk;
    put16(wlnk, 0);
    put16(wlnk, 0);
    put32(wlnk, 1); // mono channel
    put32(wlnk, wave);
    Bytes children = chunk("rgnh", rgnh);
    append(children, chunk("wlnk", wlnk));
    return list("rgn ", children);
}

/* Seven geometric wave lengths exercise repeated decoded-buffer growth and
 * reads either side of the decoder's 4096-byte block boundary. */
static void write_bank(const char *path, unsigned int version)
{
    Bytes waves, regions, table;
    put32(table, 8);
    put32(table, wave_count);
    for(unsigned int wave = 0; wave < wave_count; ++wave)
    {
        put32(table, static_cast<uint32_t>(waves.size()));
        append(waves, make_wave(wave, version));
        append(regions, make_region(wave));
    }
    Bytes insh;
    put32(insh, wave_count);
    put32(insh, 0);
    put32(insh, 0);
    Bytes instrument = chunk("insh", insh);
    append(instrument, list("lrgn", regions));
    Bytes colh;
    put32(colh, 1);
    Bytes payload{'D', 'L', 'S', ' '};
    append(payload, chunk("colh", colh));
    append(payload, list("lins", list("ins ", instrument)));
    append(payload, chunk("ptbl", table));
    append(payload, list("wvpl", waves));
    const Bytes bank = chunk("RIFF", payload);
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    TEST_ASSERT(output.good());
    output.write(reinterpret_cast<const char *>(bank.data()), bank.size());
    output.close();
    TEST_ASSERT(output.good());
}

/* Check the full decoded word stream and published span, not only successful
 * loading. The expected values come directly from PCM encoding semantics. */
static void assert_sample(fluid_synth_t *synth, unsigned int wave, unsigned int version,
                          unsigned int start)
{
    fluid_voice_t *voices[16] = {};
    fluid_synth_get_voicelist(synth, voices, 16, -1);
    unsigned int active = 0;
    for(auto *voice : voices)
    {
        if(voice == nullptr || !fluid_voice_is_on(voice))
        {
            continue;
        }
        ++active;
        TEST_ASSERT(fluid_voice_get_key(voice) == static_cast<int>(60 + wave));
        const fluid_sample_t *sample = voice->sample;
        TEST_ASSERT(sample != nullptr);
        TEST_ASSERT(sample->start == start);
        TEST_ASSERT(sample->end == start + frame_counts[wave] - 1);
        TEST_ASSERT(sample->samplerate == 44100);
        TEST_ASSERT(sample->origpitch == static_cast<int>(60 + wave));
        for(unsigned int frame = 0; frame < frame_counts[wave]; ++frame)
        {
            TEST_ASSERT(sample->data[start + frame] == expected_frame(wave, frame, version));
        }
    }
    TEST_ASSERT(active == 1);
}

/* Keep render blocks aligned across reloads and prove all selected samples
 * reach audible output rather than remaining unused bank metadata. */
static std::vector<float> observe_bank(fluid_synth_t *synth, int id, unsigned int version)
{
    TEST_SUCCESS(fluid_synth_program_select(synth, 0, id, 0, 0));
    std::vector<float> result;
    unsigned int start = 0;
    for(unsigned int wave = 0; wave < wave_count; ++wave)
    {
        TEST_SUCCESS(fluid_synth_all_sounds_off(synth, 0));
        float left[1024] = {}, right[1024] = {};
        TEST_SUCCESS(fluid_synth_write_float(synth, 1024, left, 0, 1, right, 0, 1));
        TEST_SUCCESS(fluid_synth_noteon(synth, 0, 60 + wave, 100));
        assert_sample(synth, wave, version, start);
        TEST_SUCCESS(fluid_synth_write_float(synth, 1024, left, 0, 1, right, 0, 1));
        double energy = 0;
        for(float sample : left)
        {
            TEST_ASSERT(std::isfinite(sample));
            energy += sample * sample;
        }
        TEST_ASSERT(energy > 1e-8);
        result.insert(result.end(), left, left + 1024);
        start += frame_counts[wave];
    }
    TEST_SUCCESS(fluid_synth_all_sounds_off(synth, 0));
    return result;
}

int main(void)
{
    const char *path = "test_dls_sample_storage_tmp.dls";
    write_bank(path, 0);
    fluid_settings_t *settings = new_fluid_settings();
    TEST_ASSERT(settings != nullptr);
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.reverb.active", 0));
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.chorus.active", 0));
    fluid_synth_t *synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != nullptr);
    const int id = fluid_synth_sfload(synth, path, 1);
    TEST_ASSERT(id >= 0);
    const auto original = observe_bank(synth, id, 0);
    TEST_SUCCESS(fluid_synth_sfunload(synth, id, 1));
    const int reloaded_id = fluid_synth_sfload(synth, path, 1);
    TEST_ASSERT(reloaded_id >= 0);
    const auto repeated = observe_bank(synth, reloaded_id, 0);
    TEST_ASSERT(original == repeated);
    write_bank(path, 1);
    TEST_ASSERT(fluid_synth_sfreload(synth, reloaded_id) == reloaded_id);
    const auto replaced = observe_bank(synth, reloaded_id, 1);
    for(unsigned int wave = 0; wave < wave_count; ++wave)
    {
        bool differs = false;
        for(unsigned int frame = 0; frame < 1024; ++frame)
        {
            differs |= original[wave * 1024 + frame] != replaced[wave * 1024 + frame];
        }
        TEST_ASSERT(differs);
    }
    TEST_SUCCESS(fluid_synth_sfunload(synth, reloaded_id, 1));
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    TEST_ASSERT(std::remove(path) == 0);
    return 0;
}
