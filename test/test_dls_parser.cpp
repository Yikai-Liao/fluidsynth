#include "test.h"
#include <fluidsynth.h>
#include "fluid_dls.h"
#include "fluid_sfont.h"
#include <cmath>

#include <algorithm>
#include <cstdint>
#include "fluid_voice.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

static uint32_t read_u32(const std::vector<uint8_t> &data, size_t offset)
{
    return static_cast<uint32_t>(data[offset])
           | (static_cast<uint32_t>(data[offset + 1]) << 8)
           | (static_cast<uint32_t>(data[offset + 2]) << 16)
           | (static_cast<uint32_t>(data[offset + 3]) << 24);
}

static void write_u16(std::vector<uint8_t> &data, size_t offset, uint16_t value)
{
    data[offset] = static_cast<uint8_t>(value);
    data[offset + 1] = static_cast<uint8_t>(value >> 8);
}

static void write_u32(std::vector<uint8_t> &data, size_t offset, uint32_t value)
{
    for(unsigned int i = 0; i < 4; ++i)
    {
        data[offset + i] = static_cast<uint8_t>(value >> (i * 8));
    }
}

static bool chunk_is(const std::vector<uint8_t> &data, size_t offset, const char id[4])
{
    return data[offset] == static_cast<uint8_t>(id[0])
           && data[offset + 1] == static_cast<uint8_t>(id[1])
           && data[offset + 2] == static_cast<uint8_t>(id[2])
           && data[offset + 3] == static_cast<uint8_t>(id[3]);
}

static size_t find_chunk(const std::vector<uint8_t> &data,
                         size_t start,
                         size_t end,
                         const char id[4])
{
    for(size_t pos = start; pos + 8 <= end;)
    {
        const uint32_t size = read_u32(data, pos + 4);
        const size_t payload = pos + 8;

        if(size > end - payload)
        {
            break;
        }

        if(chunk_is(data, pos, id))
        {
            return pos;
        }

        const size_t next = payload + size + (size & 1u);

        if(next > end)
        {
            break;
        }

        pos = next;
    }

    return data.size();
}

static size_t find_list(const std::vector<uint8_t> &data,
                        size_t start,
                        size_t end,
                        const char type[4])
{
    for(size_t pos = start; pos + 12 <= end;)
    {
        const uint32_t size = read_u32(data, pos + 4);
        const size_t payload = pos + 8;

        if(size < 4 || size > end - payload)
        {
            break;
        }

        if(chunk_is(data, pos, "LIST") && chunk_is(data, payload, type))
        {
            return pos;
        }

        const size_t next = payload + size + (size & 1u);

        if(next > end)
        {
            break;
        }

        pos = next;
    }

    return data.size();
}

static size_t append_first_region_cdl(std::vector<uint8_t> &data,
                                    const std::vector<uint8_t> &expression)
{
    const size_t riff_end = 8 + read_u32(data, 4);
    const size_t lins = find_list(data, 12, riff_end, "lins");
    TEST_ASSERT(lins < data.size());
    const size_t instrument = find_list(data, lins + 12,
                                        lins + 8 + read_u32(data, lins + 4), "ins ");
    TEST_ASSERT(instrument < data.size());
    const size_t lrgn = find_list(data, instrument + 12,
                                  instrument + 8 + read_u32(data, instrument + 4), "lrgn");
    TEST_ASSERT(lrgn < data.size());
    const size_t region = find_list(data, lrgn + 12,
                                    lrgn + 8 + read_u32(data, lrgn + 4), "rgn2");
    TEST_ASSERT(region < data.size());

    const size_t insert_at = region + 8 + read_u32(data, region + 4);
    std::vector<uint8_t> chunk(8 + expression.size() + (expression.size() & 1u), 0);
    std::memcpy(chunk.data(), "cdl ", 4);
    write_u32(chunk, 4, static_cast<uint32_t>(expression.size()));
    std::memcpy(chunk.data() + 8, expression.data(), expression.size());
    const uint32_t growth = static_cast<uint32_t>(chunk.size());
    data.insert(data.begin() + insert_at, chunk.begin(), chunk.end());

    for(size_t size_field : { region + 4, lrgn + 4, instrument + 4, lins + 4, size_t{4} })
    {
        write_u32(data, size_field, read_u32(data, size_field) + growth);
    }
    return insert_at + 8;
}

static void append_first_region_articulation_cdl(std::vector<uint8_t> &data,
                                                const std::vector<uint8_t> &expression)
{
    const size_t riff_end = 8 + read_u32(data, 4);
    const size_t lins = find_list(data, 12, riff_end, "lins");
    TEST_ASSERT(lins < data.size());
    const size_t instrument = find_list(data, lins + 12,
                                        lins + 8 + read_u32(data, lins + 4), "ins ");
    TEST_ASSERT(instrument < data.size());
    const size_t instrument_end = instrument + 8 + read_u32(data, instrument + 4);
    const size_t lrgn = find_list(data, instrument + 12, instrument_end, "lrgn");
    TEST_ASSERT(lrgn < data.size());
    const size_t lrgn_end = lrgn + 8 + read_u32(data, lrgn + 4);
    const size_t region = find_list(data, lrgn + 12, lrgn_end, "rgn2");
    TEST_ASSERT(region < data.size());
    const size_t region_end = region + 8 + read_u32(data, region + 4);
    size_t lart = find_list(data, region + 12, region_end, "lart");

    if(lart == data.size())
    {
        lart = find_list(data, region + 12, region_end, "lar2");
    }

    TEST_ASSERT(lart < data.size());

    const size_t insert_at = lart + 8 + read_u32(data, lart + 4);
    std::vector<uint8_t> chunk(8 + expression.size());
    std::memcpy(chunk.data(), "cdl ", 4);
    write_u32(chunk, 4, static_cast<uint32_t>(expression.size()));
    std::memcpy(chunk.data() + 8, expression.data(), expression.size());
    const uint32_t growth = static_cast<uint32_t>(chunk.size());
    data.insert(data.begin() + insert_at, chunk.begin(), chunk.end());

    for(size_t size_field : { lart + 4, region + 4, lrgn + 4, instrument + 4,
                              lins + 4, size_t{4} })
    {
        write_u32(data, size_field, read_u32(data, size_field) + growth);
    }
}

static size_t append_conditional_region_articulation(std::vector<uint8_t> &data,
                                                   const std::vector<uint8_t> &expression,
                                                   bool condition_first)
{
    const size_t lins = find_list(data, 12, 8 + read_u32(data, 4), "lins");
    TEST_ASSERT(lins < data.size());
    const size_t instrument = find_list(data, lins + 12,
                                        lins + 8 + read_u32(data, lins + 4), "ins ");
    TEST_ASSERT(instrument < data.size());
    const size_t lrgn = find_list(data, instrument + 12,
                                  instrument + 8 + read_u32(data, instrument + 4), "lrgn");
    TEST_ASSERT(lrgn < data.size());
    const size_t region = find_list(data, lrgn + 12,
                                    lrgn + 8 + read_u32(data, lrgn + 4), "rgn2");
    TEST_ASSERT(region < data.size());

    std::vector<uint8_t> chunk(12 + 28 + 8 + expression.size(), 0);
    std::memcpy(chunk.data(), "LIST", 4);
    write_u32(chunk, 4, static_cast<uint32_t>(chunk.size() - 8));
    std::memcpy(chunk.data() + 8, "lar2", 4);
    const size_t art = condition_first ? 20 + expression.size() : 12;
    const size_t cdl = condition_first ? 12 : 40;
    std::memcpy(chunk.data() + art, "art2", 4);
    write_u32(chunk, art + 4, 20);
    write_u32(chunk, art + 8, 8);  // cbSize
    write_u32(chunk, art + 12, 1); // One constant gain connection
    write_u16(chunk, art + 20, 1); // CONN_DST_GAIN
    write_u32(chunk, art + 24, static_cast<uint32_t>(-960 * 65536));
    std::memcpy(chunk.data() + cdl, "cdl ", 4);
    write_u32(chunk, cdl + 4, static_cast<uint32_t>(expression.size()));
    std::memcpy(chunk.data() + cdl + 8, expression.data(), expression.size());

    const size_t insert_at = region + 8 + read_u32(data, region + 4);
    data.insert(data.begin() + insert_at, chunk.begin(), chunk.end());
    for(size_t size_field : { region + 4, lrgn + 4, instrument + 4, lins + 4, size_t{4} })
    {
        write_u32(data, size_field, read_u32(data, size_field) + chunk.size());
    }
    return insert_at + art;
}

static std::vector<float> playing_voice_generators(const char *path, int generator)
{
    fluid_settings_t *settings = new_fluid_settings();
    TEST_ASSERT(settings != nullptr);
    fluid_synth_t *synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != nullptr);
    const int sfont_id = fluid_synth_sfload(synth, path, 1);
    TEST_ASSERT(sfont_id != FLUID_FAILED);
    TEST_SUCCESS(fluid_synth_program_select(synth, 0, sfont_id, 0, 0));
    TEST_SUCCESS(fluid_synth_noteon(synth, 0, 60, 100));

    fluid_voice_t *voices[512] = {};
    fluid_synth_get_voicelist(synth, voices, 512, -1);
    std::vector<float> values;

    for(fluid_voice_t *voice : voices)
    {
        if(voice != nullptr && fluid_voice_is_playing(voice))
        {
            values.push_back(fluid_voice_gen_get(voice, generator));
        }
    }

    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    return values;
}

static std::vector<float> playing_voice_attenuations(const char *path)
{
    return playing_voice_generators(path, GEN_ATTENUATION);
}

static void write_dls_file(const char *path, const std::vector<uint8_t> &data)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    TEST_ASSERT(output.good());
    output.write(reinterpret_cast<const char *>(data.data()),
                 static_cast<std::streamsize>(data.size()));
    output.close();
    TEST_ASSERT(output.good());
}

/* Check numeric CDL words against independent DLS 2.2 opcode expectations. */
static void assert_numeric_cdl(const std::vector<uint8_t> &data, size_t control_voice_count)
{
    /* DLS Level 2.2 v1.0 section 2.6, pp. 42-43: binary operators use
     * top-of-stack X and previous Y; logical TRUE is the all-ones word.
     * Keep all inputs ordinary: nonzero divisors and no arithmetic overflow. */
    struct NumericCdlCase
    {
        uint16_t opcode;
        uint32_t y;
        uint32_t x;
        uint32_t expected;
    };
    const NumericCdlCase numeric_cdl_cases[] = {
        {0x0001, 12, 10, 8},          // Bitwise AND
        {0x0002, 12, 10, 14},         // Bitwise OR
        {0x0003, 12, 10, 6},          // Bitwise XOR
        {0x0004, 12, 10, 22},         // ADD
        {0x0005, 3, 17, 14},          // SUBTRACT: X - Y
        {0x0006, 12, 10, 120},        // MULTIPLY
        {0x0007, 3, 17, 5},           // DIVIDE: X / Y, integer quotient
        {0x0007, 17, 3, 0},           // DIVIDE: reversed operands
        {0x0008, 4, 0, 0},            // Logical AND: X is false
        {0x0009, 4, 0, UINT32_MAX}     // Logical OR: X is false, Y is true
    };
    const char *numeric_cdl_path = "test_dls_numeric_cdl_tmp.dls";
    for(const auto &numeric_case : numeric_cdl_cases)
    {
        for(bool matches : { true, false })
        {
            std::vector<uint8_t> expression(22);
            write_u16(expression, 0, 0x0010); // CONST Y
            write_u32(expression, 2, numeric_case.y);
            write_u16(expression, 6, 0x0010); // CONST X
            write_u32(expression, 8, numeric_case.x);
            write_u16(expression, 12, numeric_case.opcode);
            write_u16(expression, 14, 0x0010); // CONST expected result
            write_u32(expression, 16, matches ? numeric_case.expected
                                             : numeric_case.expected ^ uint32_t{1});
            write_u16(expression, 20, 0x000e); // EQ, testing the complete word

            std::vector<uint8_t> numeric_dls(data);
            append_first_region_cdl(numeric_dls, expression);
            write_dls_file(numeric_cdl_path, numeric_dls);
            const size_t voices = playing_voice_attenuations(numeric_cdl_path).size();
            TEST_ASSERT(std::remove(numeric_cdl_path) == 0);
            /* Both files are valid. Only the unequal result skips a region. */
            TEST_ASSERT(voices + (matches ? 0u : 1u) == control_voice_count);
        }
    }
}

/* Render bounded audio, so release assertions observe the DSP lifecycle too. */
static void render_dls_frames(fluid_synth_t *synth, int frames)
{
    float left[512], right[512];
    while(frames > 0)
    {
        const int count = std::min(frames, 512);
        TEST_SUCCESS(fluid_synth_write_float(synth, count, left, 0, 1, right, 0, 1));
        frames -= count;
    }
}

/* PGAL is a supported legacy extension, not part of the normative DLS format.
 * Its on-disk compatibility layout comes from the audited pre-refactor loader. */
static std::vector<uint8_t> make_pgal_alias_bank(const std::vector<uint8_t> &data,
                                               bool versioned)
{
    const size_t table = versioned ? 12 : 8;
    std::vector<uint8_t> pgal(table + 128 + 4 + 16, 0);
    std::memcpy(pgal.data(), "pgal", 4);
    write_u32(pgal, 4, static_cast<uint32_t>(pgal.size() - 8));
    // The explicit version is zero. The legacy form starts with notes 0,1,2,3.
    for(size_t note = 0; note < 128; ++note)
    {
        pgal[table + note] = static_cast<uint8_t>(note);
    }
    write_u32(pgal, table + 128, 2);
    const uint16_t banks[] = {5, 130};
    const uint8_t programs[] = {17, 23};
    for(size_t i = 0; i < 2; ++i)
    {
        const size_t record = table + 132 + i * 8;
        write_u16(pgal, record, banks[i]);
        pgal[record + 2] = programs[i];
        // Remaining fields select original bank 0 / program 0, with zero pads.
    }
    std::vector<uint8_t> result(data);
    const size_t riff_end = 8 + read_u32(data, 4);
    result.insert(result.begin() + riff_end, pgal.begin(), pgal.end());
    write_u32(result, 4, read_u32(data, 4) + pgal.size());
    return result;
}

struct PgalObservation
{
    std::vector<const fluid_sample_t *> samples;
    std::vector<float> pcm;
};

/* Observe selection and complete audio onset at block-aligned positions. */
static PgalObservation observe_pgal_preset(fluid_synth_t *synth, int id,
                                          int bank, int program)
{
    TEST_SUCCESS(fluid_synth_all_sounds_off(synth, 0));
    render_dls_frames(synth, 512);
    TEST_SUCCESS(fluid_synth_program_select(synth, 0, id, bank, program));
    TEST_SUCCESS(fluid_synth_noteon(synth, 0, 60, 100));
    fluid_voice_t *voices[512] = {};
    fluid_synth_get_voicelist(synth, voices, 512, -1);
    PgalObservation result;
    for(auto *voice : voices)
    {
        if(voice != nullptr && fluid_voice_is_on(voice))
        {
            TEST_ASSERT(fluid_voice_get_key(voice) == 60);
            TEST_ASSERT(fluid_voice_get_actual_key(voice) == 60);
            result.samples.push_back(voice->sample);
        }
    }
    TEST_ASSERT(!result.samples.empty());
    std::sort(result.samples.begin(), result.samples.end(),
              [](const fluid_sample_t *a, const fluid_sample_t *b)
              { return a->start < b->start; });
    result.pcm.resize(4096);
    TEST_SUCCESS(fluid_synth_write_float(synth, 2048, result.pcm.data(), 0, 2,
                                         result.pcm.data(), 1, 2));
    double energy = 0;
    for(float value : result.pcm)
    {
        TEST_ASSERT(std::isfinite(value));
        energy += double(value) * value;
    }
    TEST_ASSERT(energy > 1e-8);
    return result;
}

/* Alias records must publish their own metadata but play the original preset.
 * Both supported header layouts must preserve exact sample and audio identity. */
static void assert_pgal_melodic_aliases(const std::vector<uint8_t> &data)
{
    const char *path = "test_dls_pgal_melodic_alias_tmp.dls";
    for(bool versioned : {true, false})
    {
        write_dls_file(path, make_pgal_alias_bank(data, versioned));
        fluid_settings_t *settings = new_fluid_settings();
        TEST_ASSERT(settings != nullptr);
        TEST_SUCCESS(fluid_settings_setstr(settings, "synth.midi-bank-select", "mma"));
        TEST_SUCCESS(fluid_settings_setint(settings, "synth.reverb.active", 0));
        TEST_SUCCESS(fluid_settings_setint(settings, "synth.chorus.active", 0));
        fluid_synth_t *synth = new_fluid_synth(settings);
        TEST_ASSERT(synth != nullptr);
        const int id = fluid_synth_sfload(synth, path, 1);
        TEST_ASSERT(id >= 0);
        fluid_sfont_t *sfont = fluid_synth_get_sfont_by_id(synth, id);
        TEST_ASSERT(sfont != nullptr);
        const PgalObservation original = observe_pgal_preset(synth, id, 0, 0);
        const int banks[] = {5, 130};
        const int programs[] = {17, 23};
        for(size_t i = 0; i < 2; ++i)
        {
            fluid_preset_t *preset = fluid_sfont_get_preset(sfont, banks[i], programs[i]);
            TEST_ASSERT(preset != nullptr);
            TEST_ASSERT(fluid_preset_get_banknum(preset) == banks[i]);
            TEST_ASSERT(fluid_preset_get_num(preset) == programs[i]);
            const PgalObservation alias = observe_pgal_preset(synth, id, banks[i], programs[i]);
            TEST_ASSERT(alias.samples == original.samples);
            TEST_ASSERT(alias.pcm == original.pcm);
        }
        TEST_SUCCESS(fluid_synth_all_sounds_off(synth, 0));
        render_dls_frames(synth, 512);
        TEST_ASSERT(fluid_synth_get_active_voice_count(synth) == 0);
        delete_fluid_synth(synth);
        delete_fluid_settings(settings);
        TEST_ASSERT(std::remove(path) == 0);
    }
}

/* Compare rendered note behavior without reverb/chorus tails across bank loads. */
static std::vector<float> capture_dls_bank_audio(fluid_synth_t *synth, int sfont)
{
    TEST_SUCCESS(fluid_synth_all_sounds_off(synth, 0));
    render_dls_frames(synth, 512);
    TEST_SUCCESS(fluid_synth_program_select(synth, 0, sfont, 0, 0));
    TEST_SUCCESS(fluid_synth_noteon(synth, 0, 60, 100));
    std::vector<float> pcm(2048);
    TEST_SUCCESS(fluid_synth_write_float(synth, 1024, pcm.data(), 0, 2,
                                         pcm.data(), 1, 2));
    double energy = 0;
    for(float sample : pcm)
    {
        TEST_ASSERT(std::isfinite(sample));
        energy += sample * sample;
    }
    TEST_ASSERT(energy > 1e-8);
    TEST_SUCCESS(fluid_synth_all_sounds_off(synth, 0));
    render_dls_frames(synth, 512);
    return pcm;
}

/* DLS 2.2 section 2.6: a false top-level condition excludes the whole bank
 * with an error result; it is a valid file, not a malformed-input test. */
static void assert_top_level_cdl(const std::vector<uint8_t> &data)
{
    const char *path = "test_dls_top_level_cdl_tmp.dls";
    write_dls_file(path, data);
    fluid_settings_t *settings = new_fluid_settings();
    TEST_ASSERT(settings != nullptr);
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.reverb.active", 0));
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.chorus.active", 0));
    fluid_synth_t *synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != nullptr);
    const int original = fluid_synth_sfload(synth, path, 0);
    TEST_ASSERT(original >= 0);
    const auto reference = capture_dls_bank_audio(synth, original);
    for(bool condition_first : { true, false })
    {
        for(bool applicable : { true, false })
        {
            std::vector<uint8_t> condition(14, 0);
            std::memcpy(condition.data(), "cdl ", 4);
            write_u32(condition, 4, 6);
            write_u16(condition, 8, 0x0010); // CONST
            write_u32(condition, 10, applicable ? 1 : 0);
            std::vector<uint8_t> conditional(data);
            const size_t at = condition_first ? 12 : 8 + read_u32(data, 4);
            conditional.insert(conditional.begin() + at, condition.begin(), condition.end());
            write_u32(conditional, 4, read_u32(data, 4) + condition.size());
            write_dls_file(path, conditional);
            const int loaded = fluid_synth_sfload(synth, path, 0);
            if(applicable)
            {
                TEST_ASSERT(loaded >= 0);
                TEST_ASSERT(fluid_synth_sfcount(synth) == 2);
                TEST_ASSERT(capture_dls_bank_audio(synth, loaded) == reference);
                TEST_SUCCESS(fluid_synth_sfunload(synth, loaded, 0));
            }
            else
            {
                TEST_ASSERT(loaded == FLUID_FAILED);
            }
            TEST_ASSERT(fluid_synth_sfcount(synth) == 1);
            TEST_ASSERT(fluid_synth_get_sfont_by_id(synth, original) != nullptr);
            TEST_ASSERT(capture_dls_bank_audio(synth, original) == reference);
        }
    }
    TEST_SUCCESS(fluid_synth_sfunload(synth, original, 0));
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    TEST_ASSERT(std::remove(path) == 0);
}

static size_t assert_dls_held_keys(fluid_synth_t *synth, int key, int actual_key,
                                   bool sustained = false)
{
    fluid_voice_t *voices[512] = {};
    fluid_synth_get_voicelist(synth, voices, 512, -1);
    size_t count = 0;
    for(fluid_voice_t *voice : voices)
    {
        if(voice != nullptr && (sustained ? fluid_voice_is_sustained(voice)
                                         : fluid_voice_is_on(voice)))
        {
            TEST_ASSERT(fluid_voice_get_key(voice) == key);
            TEST_ASSERT(fluid_voice_get_actual_key(voice) == actual_key);
            ++count;
        }
    }
    return count;
}

static void assert_dls_note_lifecycle(fluid_synth_t *synth, int channel, int actual_key)
{
    TEST_SUCCESS(fluid_synth_noteon(synth, channel, 60, 100));
    render_dls_frames(synth, 512);
    TEST_ASSERT(assert_dls_held_keys(synth, 60, actual_key) > 0);
    TEST_SUCCESS(fluid_synth_noteoff(synth, channel, 60));
    TEST_ASSERT(assert_dls_held_keys(synth, 60, actual_key) == 0);
    render_dls_frames(synth, 44100 * 5);
    TEST_ASSERT(fluid_synth_get_active_voice_count(synth) == 0);

    TEST_SUCCESS(fluid_synth_cc(synth, channel, 64, 127));
    TEST_SUCCESS(fluid_synth_noteon(synth, channel, 60, 100));
    render_dls_frames(synth, 512);
    const size_t held = assert_dls_held_keys(synth, 60, actual_key);
    TEST_ASSERT(held > 0);
    TEST_SUCCESS(fluid_synth_noteoff(synth, channel, 60));
    render_dls_frames(synth, 512);
    TEST_ASSERT(assert_dls_held_keys(synth, 60, actual_key, true) == held);
    TEST_SUCCESS(fluid_synth_cc(synth, channel, 64, 0));
    render_dls_frames(synth, 44100 * 5);
    TEST_ASSERT(fluid_synth_get_active_voice_count(synth) == 0);
}

static void assert_dls_mono_lifecycle(fluid_synth_t *synth, int channel,
                                      int actual_60, int actual_64)
{
    TEST_SUCCESS(fluid_synth_cc(synth, channel, 126, 0));
    TEST_SUCCESS(fluid_synth_set_legato_mode(synth, channel,
                 FLUID_CHANNEL_LEGATO_MODE_MULTI_RETRIGGER));
    TEST_SUCCESS(fluid_synth_noteon(synth, channel, 60, 100));
    render_dls_frames(synth, 512);
    TEST_ASSERT(assert_dls_held_keys(synth, 60, actual_60) > 0);
    TEST_SUCCESS(fluid_synth_noteon(synth, channel, 64, 100));
    render_dls_frames(synth, 512);
    TEST_ASSERT(assert_dls_held_keys(synth, 64, actual_64) > 0);
    TEST_SUCCESS(fluid_synth_noteoff(synth, channel, 64));
    render_dls_frames(synth, 512);
    TEST_ASSERT(assert_dls_held_keys(synth, 60, actual_60) > 0);
    TEST_SUCCESS(fluid_synth_noteoff(synth, channel, 60));
    render_dls_frames(synth, 44100 * 5);
    TEST_ASSERT(fluid_synth_get_active_voice_count(synth) == 0);
}

/* Observe real DSP output as well as voice metadata, without effects tails. */
static void assert_dls_steal_audio(fluid_synth_t *synth)
{
    float left[512] = {}, right[512] = {};
    TEST_SUCCESS(fluid_synth_write_float(synth, 512, left, 0, 1, right, 0, 1));
    double energy = 0;
    for(float sample : left)
    {
        TEST_ASSERT(std::isfinite(sample));
        energy += sample * sample;
    }
    TEST_ASSERT(energy > 1e-8);
}

/* Two active layers fill polyphony: no stopped voice is available. Each
 * transition must replace active working state while the old render voice
 * retains its sample until the queued stop has actually been rendered. */
static void assert_dls_active_steal(fluid_synth_t *synth)
{
    TEST_SUCCESS(fluid_synth_set_polyphony(synth, 2));
    TEST_SUCCESS(fluid_synth_cc(synth, 0, 127, 0));
    std::vector<short> pcm(8192);
    for(size_t i = 0; i < pcm.size(); ++i)
    {
        pcm[i] = (i % 64 < 32) ? 8000 : -8000;
    }
    fluid_sample_t *ordinary_sample = new_fluid_sample();
    TEST_ASSERT(ordinary_sample != nullptr);
    TEST_SUCCESS(fluid_sample_set_sound_data(ordinary_sample, pcm.data(), nullptr,
                 static_cast<unsigned int>(pcm.size()), 44100, 1));
    TEST_SUCCESS(fluid_sample_set_pitch(ordinary_sample, 67, 0));

    TEST_SUCCESS(fluid_synth_noteon(synth, 0, 60, 100));
    assert_dls_steal_audio(synth);
    TEST_ASSERT(assert_dls_held_keys(synth, 60, 30) == 2);
    fluid_voice_t *mapped[2] = {};
    fluid_synth_get_voicelist(synth, mapped, 2, -1);
    const void *context = mapped[0]->note_context;
    TEST_ASSERT(context != nullptr);
    for(auto *voice : mapped)
    {
        TEST_ASSERT(voice != nullptr);
        TEST_ASSERT(fluid_voice_is_on(voice));
        TEST_ASSERT(voice->sample != nullptr);
        TEST_ASSERT(!voice->can_access_rvoice);
        TEST_ASSERT(voice->can_access_overflow_rvoice);
        TEST_ASSERT(voice->note_resolver != nullptr);
        TEST_ASSERT(voice->note_context == context);
    }

    fluid_voice_t *ordinary[2] = {};
    for(int i = 0; i < 2; ++i)
    {
        ordinary[i] = fluid_synth_alloc_voice(synth, ordinary_sample, 0, 67, 100);
        TEST_ASSERT(ordinary[i] != nullptr);
        TEST_ASSERT(std::find(mapped, mapped + 2, ordinary[i]) != mapped + 2);
        TEST_ASSERT(ordinary[i]->sample == ordinary_sample);
        TEST_ASSERT(ordinary[i]->overflow_sample != nullptr);
        TEST_ASSERT(ordinary[i]->overflow_sample != ordinary_sample);
        TEST_ASSERT(!ordinary[i]->can_access_overflow_rvoice);
        TEST_ASSERT(ordinary[i]->note_resolver == nullptr);
        TEST_ASSERT(ordinary[i]->note_context == nullptr);
        TEST_ASSERT(ordinary[i]->default_actual_key == 67);
        fluid_synth_start_voice(synth, ordinary[i]);
    }
    TEST_ASSERT(ordinary[0] != ordinary[1]);
    TEST_ASSERT(ordinary_sample->refcount == 2);
    assert_dls_steal_audio(synth);
    TEST_ASSERT(assert_dls_held_keys(synth, 67, 67) == 2);
    for(auto *voice : ordinary)
    {
        TEST_ASSERT(fluid_voice_is_on(voice));
        TEST_ASSERT(!voice->can_access_rvoice);
        TEST_ASSERT(voice->can_access_overflow_rvoice);
        TEST_ASSERT(voice->overflow_sample == nullptr);
    }

    /* The new mapped note now steals both still-active ordinary allocations.
     * Their sample must remain alive in overflow until the next render. */
    TEST_SUCCESS(fluid_synth_noteon(synth, 0, 64, 100));
    TEST_ASSERT(assert_dls_held_keys(synth, 64, 32) == 2);
    TEST_ASSERT(ordinary_sample->refcount == 2);
    for(auto *voice : ordinary)
    {
        TEST_ASSERT(voice->sample != ordinary_sample);
        TEST_ASSERT(voice->overflow_sample == ordinary_sample);
        TEST_ASSERT(voice->note_resolver != nullptr);
        TEST_ASSERT(voice->note_context == context);
        TEST_ASSERT(voice->default_actual_key == 32);
    }
    assert_dls_steal_audio(synth);
    /* A synth API entry reconciles renderer completion before inspecting refs. */
    TEST_ASSERT(assert_dls_held_keys(synth, 64, 32) == 2);
    TEST_ASSERT(ordinary_sample->refcount == 0);
    TEST_SUCCESS(fluid_synth_noteoff(synth, 0, 64));
    render_dls_frames(synth, 44100 * 5);
    TEST_ASSERT(fluid_synth_get_active_voice_count(synth) == 0);
    for(auto *voice : ordinary)
    {
        TEST_ASSERT(voice->sample == nullptr);
        TEST_ASSERT(voice->overflow_sample == nullptr);
        TEST_ASSERT(voice->note_resolver == nullptr);
        TEST_ASSERT(voice->note_context == nullptr);
    }
    TEST_ASSERT(ordinary_sample->refcount == 0);
    delete_fluid_sample(ordinary_sample);
}

/* Exactly one selected region receives CC1; the sibling is a zero-delta control. */
static void assert_region_modulation_delta(const std::vector<fluid_voice_t *> &held,
                                           const std::vector<fluid_real_t> &initial,
                                           double expected_delta)
{
    TEST_ASSERT(held.size() == 2);
    std::vector<double> observed;
    for(size_t i = 0; i < held.size(); ++i)
    {
        TEST_ASSERT(fluid_voice_is_on(held[i]));
        observed.push_back(held[i]->gen[GEN_FINETUNE].mod - initial[i]);
    }
    std::sort(observed.begin(), observed.end());
    TEST_ASSERT(std::fabs(observed[0]) < 0.001);
    TEST_ASSERT(std::fabs(observed[1] - expected_delta) < 0.001);
}

/* DLS 2.2 section 1.6.2 sums scaled connections; section 1.6.4 normalizes
 * unipolar CC1 as CC1/128. A 1200-cent scale therefore adds exactly 600 cents
 * at CC1=64, independently of the default playback-key mapping. */
static void assert_dls_articulation_modulator(fluid_synth_t *synth)
{
    TEST_SUCCESS(fluid_synth_cc(synth, 0, 126, 0));
    TEST_SUCCESS(fluid_synth_set_legato_mode(synth, 0,
                 FLUID_CHANNEL_LEGATO_MODE_RETRIGGER));
    TEST_SUCCESS(fluid_synth_cc(synth, 0, 1, 0));
    struct Step { int key; bool note_on; };
    const Step steps[] = {{60, true}, {64, true}, {60, false}};
    for(const auto &step : steps)
    {
        const int key = step.key;
        if(step.note_on)
        {
            TEST_SUCCESS(fluid_synth_noteon(synth, 0, key, 100));
        }
        else
        {
            TEST_SUCCESS(fluid_synth_noteoff(synth, 0, 64));
        }
        render_dls_frames(synth, 512);
        TEST_ASSERT(assert_dls_held_keys(synth, key, key / 2) > 0);
        fluid_voice_t *voices[512] = {};
        fluid_synth_get_voicelist(synth, voices, 512, -1);
        std::vector<fluid_voice_t *> held;
        std::vector<fluid_real_t> initial;
        for(auto *voice : voices)
        {
            if(voice != nullptr && fluid_voice_is_on(voice))
            {
                held.push_back(voice);
                initial.push_back(voice->gen[GEN_FINETUNE].mod);
            }
        }
        /* Exercise both live modulation and return to the original amount on
         * freshly allocated and mono-retriggered mapped voices. */
        for(int cc : {64, 0})
        {
            TEST_SUCCESS(fluid_synth_cc(synth, 0, 1, cc));
            render_dls_frames(synth, 512);
            TEST_ASSERT(assert_dls_held_keys(synth, key, key / 2) == held.size());
            assert_region_modulation_delta(held, initial, cc * (1200.0 / 128.0));
        }
    }
    TEST_SUCCESS(fluid_synth_all_sounds_off(synth, 0));
    render_dls_frames(synth, 512);
    TEST_ASSERT(fluid_synth_get_active_voice_count(synth) == 0);
}

/* Mode 0 releases the prior allocation and starts a fresh mapped voice. */
static void assert_dls_retrigger_lifecycle(fluid_synth_t *synth)
{
    TEST_SUCCESS(fluid_synth_cc(synth, 0, 126, 0));
    TEST_SUCCESS(fluid_synth_set_legato_mode(synth, 0,
                 FLUID_CHANNEL_LEGATO_MODE_RETRIGGER));
    TEST_SUCCESS(fluid_synth_noteon(synth, 0, 60, 100));
    render_dls_frames(synth, 512);
    TEST_ASSERT(assert_dls_held_keys(synth, 60, 30) > 0);
    fluid_voice_t *original[512] = {};
    fluid_synth_get_voicelist(synth, original, 512, -1);
    TEST_SUCCESS(fluid_synth_noteon(synth, 0, 64, 100));
    TEST_ASSERT(assert_dls_held_keys(synth, 64, 32) > 0);
    for(auto *voice : original)
    {
        if(voice != nullptr)
        {
            TEST_ASSERT(!fluid_voice_is_on(voice));
            TEST_ASSERT(fluid_voice_get_key(voice) == 60);
            TEST_ASSERT(fluid_voice_get_actual_key(voice) == 30);
        }
    }
    render_dls_frames(synth, 512);
    fluid_voice_t *retriggered[512] = {};
    fluid_synth_get_voicelist(synth, retriggered, 512, -1);
    TEST_SUCCESS(fluid_synth_noteoff(synth, 0, 64));
    TEST_ASSERT(assert_dls_held_keys(synth, 60, 30) > 0);
    for(auto *voice : retriggered)
    {
        if(voice != nullptr && fluid_voice_get_key(voice) == 64)
        {
            TEST_ASSERT(!fluid_voice_is_on(voice));
            TEST_ASSERT(fluid_voice_get_actual_key(voice) == 32);
        }
    }
    TEST_SUCCESS(fluid_synth_noteoff(synth, 0, 60));
    render_dls_frames(synth, 44100 * 5);
    TEST_ASSERT(fluid_synth_get_active_voice_count(synth) == 0);
    TEST_SUCCESS(fluid_synth_cc(synth, 0, 127, 0));
}

/* set_gen supplies an offset: the fixture's KEYNUM base remains -1.
 * These assertions check the public effective-key contract, not realtime pitch
 * changes: KEYNUM is documented as a non-realtime generator. */
static void assert_dls_keynum_overrides(fluid_synth_t *synth, int fallback_key)
{
    for(float initial_offset : { 0.0f, 49.0f, -1.0f })
    {
        TEST_SUCCESS(fluid_synth_set_gen(synth, 0, GEN_KEYNUM, initial_offset));
        TEST_SUCCESS(fluid_synth_noteon(synth, 0, 60, 100));
        render_dls_frames(synth, 512);
        const int initial_key = initial_offset == 49.0f ? 48 : fallback_key;
        TEST_ASSERT(assert_dls_held_keys(synth, 60, initial_key) > 0);

        fluid_voice_t *voices[512] = {};
        fluid_synth_get_voicelist(synth, voices, 512, -1);
        for(fluid_voice_t *voice : voices)
        {
            if(voice != nullptr && fluid_voice_is_on(voice))
            {
                TEST_ASSERT(fluid_voice_gen_get(voice, GEN_KEYNUM) == -1.0f);
            }
        }

        /* Positive effective override, zero key, negative fallback, reset. */
        for(float offset : { 65.0f, 1.0f, -1.0f, 0.0f })
        {
            TEST_SUCCESS(fluid_synth_set_gen(synth, 0, GEN_KEYNUM, offset));
            const int expected = offset >= 1.0f ? static_cast<int>(offset - 1.0f)
                                               : fallback_key;
            TEST_ASSERT(assert_dls_held_keys(synth, 60, expected) > 0);
        }
        TEST_SUCCESS(fluid_synth_noteoff(synth, 0, 60));
        render_dls_frames(synth, 44100 * 5);
        TEST_ASSERT(fluid_synth_get_active_voice_count(synth) == 0);
    }
}

static void assert_dls_pressure_and_reuse(fluid_synth_t *synth)
{
    TEST_SUCCESS(fluid_synth_set_legato_mode(synth, 0,
                 FLUID_CHANNEL_LEGATO_MODE_MULTI_RETRIGGER));
    TEST_SUCCESS(fluid_synth_cc(synth, 0, 126, 0));
    TEST_SUCCESS(fluid_synth_key_pressure(synth, 0, 60, 0));
    TEST_SUCCESS(fluid_synth_key_pressure(synth, 0, 64, 64));
    TEST_SUCCESS(fluid_synth_noteon(synth, 0, 60, 100));
    render_dls_frames(synth, 512);
    fluid_mod_t *pressure = new_fluid_mod();
    TEST_ASSERT(pressure != nullptr);
    fluid_mod_set_source1(pressure, FLUID_MOD_KEYPRESSURE,
                         FLUID_MOD_GC | FLUID_MOD_LINEAR | FLUID_MOD_UNIPOLAR
                         | FLUID_MOD_POSITIVE);
    fluid_mod_set_source2(pressure, FLUID_MOD_NONE, 0);
    fluid_mod_set_dest(pressure, GEN_COARSETUNE);
    fluid_mod_set_amount(pressure, 128);
    fluid_voice_t *voices[512] = {};
    fluid_synth_get_voicelist(synth, voices, 512, -1);
    std::vector<fluid_voice_t *> held;
    std::vector<fluid_real_t> before;
    for(fluid_voice_t *voice : voices)
    {
        if(voice != nullptr && fluid_voice_is_on(voice))
        {
            fluid_voice_add_mod(voice, pressure, FLUID_VOICE_OVERWRITE);
            held.push_back(voice);
            before.push_back(voice->gen[GEN_COARSETUNE].mod);
        }
    }
    TEST_ASSERT(!held.empty());
    delete_fluid_mod(pressure);
    TEST_SUCCESS(fluid_synth_noteon(synth, 0, 64, 100));
    render_dls_frames(synth, 512);
    for(size_t i = 0; i < held.size(); ++i)
    {
        TEST_ASSERT(fluid_voice_is_on(held[i]));
        TEST_ASSERT(fluid_voice_get_key(held[i]) == 64);
        TEST_ASSERT(fluid_voice_get_actual_key(held[i]) == 32);
        TEST_ASSERT(std::fabs(held[i]->gen[GEN_COARSETUNE].mod - before[i] - 64.0) < 0.001);
    }
    TEST_SUCCESS(fluid_synth_all_sounds_off(synth, 0));
    render_dls_frames(synth, 512);

    /* The fixture has two layers: keep both slots, then reuse each normally. */
    TEST_SUCCESS(fluid_synth_cc(synth, 0, 127, 0));
    TEST_SUCCESS(fluid_synth_set_polyphony(synth, 2));
    TEST_SUCCESS(fluid_synth_noteon(synth, 0, 60, 100));
    render_dls_frames(synth, 512);
    fluid_voice_t *mapped[2] = {};
    fluid_synth_get_voicelist(synth, mapped, 2, -1);
    for(fluid_voice_t *voice : mapped)
    {
        TEST_ASSERT(voice != nullptr && voice->note_resolver != nullptr);
    }
    fluid_sample_t *sample = mapped[0]->sample;
    TEST_SUCCESS(fluid_synth_all_sounds_off(synth, 0));
    render_dls_frames(synth, 512);
    fluid_voice_t *ordinary[2] = {};
    for(int i = 0; i < 2; ++i)
    {
        ordinary[i] = fluid_synth_alloc_voice(synth, sample, 0, 67 + i, 100);
        TEST_ASSERT(ordinary[i] != nullptr);
        TEST_ASSERT(ordinary[i] == mapped[0] || ordinary[i] == mapped[1]);
        TEST_ASSERT(ordinary[i]->note_resolver == nullptr && ordinary[i]->note_context == nullptr);
        TEST_ASSERT(ordinary[i]->default_actual_key == 67 + i);
        TEST_ASSERT(fluid_voice_get_key(ordinary[i]) == 67 + i);
        TEST_ASSERT(fluid_voice_get_actual_key(ordinary[i]) == 67 + i);
        fluid_synth_start_voice(synth, ordinary[i]);
    }
    TEST_ASSERT(ordinary[0] != ordinary[1]);
    TEST_SUCCESS(fluid_synth_noteoff(synth, 0, 67));
    TEST_SUCCESS(fluid_synth_noteoff(synth, 0, 68));
    render_dls_frames(synth, 44100 * 5);
    TEST_ASSERT(fluid_synth_get_active_voice_count(synth) == 0);
}

/* Metadata alone must not install a playback-key resolver on identity presets. */
static void assert_unmapped_preset_paths(const std::vector<uint8_t> &data, size_t insh,
                                         const std::vector<uint8_t> &alias_chunk, const char *path)
{
    const size_t riff_end = 8 + read_u32(data, 4);
    /* A drum without aliases and a melodic preset in an aliased bank both
     * keep the identity path. Bank metadata must not enable mapping alone. */
    for(bool drums : { true, false })
    {
        std::vector<uint8_t> unmapped(data);
        if(drums)
        {
            write_u32(unmapped, insh + 12, 0x80000000u);
        }
        else
        {
            unmapped.insert(unmapped.begin() + riff_end,
                            alias_chunk.begin(), alias_chunk.end());
            write_u32(unmapped, 4, read_u32(data, 4) + alias_chunk.size());
        }
        write_dls_file(path, unmapped);
        fluid_settings_t *settings = new_fluid_settings();
        TEST_ASSERT(settings != nullptr);
        fluid_synth_t *synth = new_fluid_synth(settings);
        TEST_ASSERT(synth != nullptr);
        const int unmapped_sfont = fluid_synth_sfload(synth, path, 1);
        TEST_ASSERT(unmapped_sfont >= 0);
        TEST_SUCCESS(fluid_synth_set_channel_type(synth, 0,
                     drums ? CHANNEL_TYPE_DRUM : CHANNEL_TYPE_MELODIC));
        TEST_SUCCESS(fluid_synth_program_select(synth, 0, unmapped_sfont,
                     drums ? 128 : 0, 0));
        TEST_SUCCESS(fluid_synth_noteon(synth, 0, 60, 100));
        render_dls_frames(synth, 512);
        TEST_ASSERT(assert_dls_held_keys(synth, 60, 60) > 0);
        fluid_voice_t *identity_voices[512] = {};
        fluid_synth_get_voicelist(synth, identity_voices, 512, -1);
        for(auto *voice : identity_voices)
        {
            if(voice != nullptr && fluid_voice_is_on(voice))
            {
                TEST_ASSERT(voice->note_resolver == nullptr);
                TEST_ASSERT(voice->note_context == nullptr);
            }
        }
        TEST_SUCCESS(fluid_synth_noteoff(synth, 0, 60));
        render_dls_frames(synth, 44100 * 5);
        TEST_ASSERT(fluid_synth_get_active_voice_count(synth) == 0);
        assert_dls_mono_lifecycle(synth, 0, 60, 64);
        delete_fluid_synth(synth);
        delete_fluid_settings(settings);
    }
}

/* Observe the actual read callback, not only the eventual load result:
 * rejecting after reading a sibling or padding is too late. */
static fluid_sfloader_callback_read_t observed_read;
static fluid_sfloader_callback_tell_t observed_tell;
static fluid_long_long_t observed_begin, observed_end;

static int assert_bounded_read(void *buffer, fluid_long_long_t count, void *handle)
{
    const fluid_long_long_t position = observed_tell(handle);
    if(position >= observed_begin && position <= observed_end)
    {
        TEST_ASSERT(count <= observed_end - position);
    }
    return observed_read(buffer, count, handle);
}

static size_t make_wave_chunks_8bit(std::vector<uint8_t> &data, size_t start, size_t end)
{
    size_t formats = 0;

    for(size_t pos = start; pos + 8 <= end;)
    {
        const uint32_t size = read_u32(data, pos + 4);
        const size_t payload = pos + 8;

        if(size > end - payload)
        {
            break;
        }

        if(chunk_is(data, pos, "LIST") && size >= 4)
        {
            formats += make_wave_chunks_8bit(data, payload + 4, payload + size);
        }
        else if(chunk_is(data, pos, "data") && size > 0)
        {
            data[payload] = 0; /* exercise the signed -128 endpoint */
        }
        else if(chunk_is(data, pos, "fmt ") && size >= 16)
        {
            const uint32_t sample_rate = read_u32(data, payload + 4);
            write_u32(data, payload + 8, sample_rate);
            write_u16(data, payload + 12, 1);
            write_u16(data, payload + 14, 8);
            formats++;
        }

        const size_t next = payload + size + (size & 1u);

        if(next > end)
        {
            break;
        }

        pos = next;
    }

    return formats;
}

int main(void)
{
    std::ifstream input(TEST_DLS, std::ios::binary);
    TEST_ASSERT(input.good());
    std::vector<uint8_t> data;

    for(char ch; input.get(ch);)
    {
        data.push_back(static_cast<uint8_t>(ch));
    }

    TEST_ASSERT(data.size() >= 12);
    const uint32_t riff_size = read_u32(data, 4);
    TEST_ASSERT(riff_size <= data.size() - 8);
    TEST_ASSERT(make_wave_chunks_8bit(data, 12, 8 + riff_size) > 0);

    const std::string path = "test_dls_8bit_pcm_tmp.dls";
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    TEST_ASSERT(output.good());
    output.write(reinterpret_cast<const char *>(data.data()), static_cast<std::streamsize>(data.size()));
    output.close();
    TEST_ASSERT(output.good());

    fluid_settings_t *settings = new_fluid_settings();
    TEST_ASSERT(settings != nullptr);
    fluid_synth_t *synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != nullptr);
    TEST_ASSERT(fluid_synth_sfload(synth, path.c_str(), 1) != FLUID_FAILED);

    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    TEST_ASSERT(std::remove(path.c_str()) == 0);

    assert_top_level_cdl(data);
    assert_pgal_melodic_aliases(data);

    /* A false conditional articulation list is skipped, not a bad SoundFont. */
    std::vector<uint8_t> false_cdl(6);
    write_u16(false_cdl, 0, 0x0010); /* DLS_CDL_CONST */
    write_u32(false_cdl, 2, 0);
    std::vector<uint8_t> bypassed_lart(data);
    append_first_region_articulation_cdl(bypassed_lart, false_cdl);
    const std::string bypassed_lart_path = "test_dls_lart_bypass_tmp.dls";
    write_dls_file(bypassed_lart_path.c_str(), bypassed_lart);
    settings = new_fluid_settings();
    TEST_ASSERT(settings != nullptr);
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != nullptr);
    const int bypassed_lart_sfont = fluid_synth_sfload(synth, bypassed_lart_path.c_str(), 1);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    TEST_ASSERT(std::remove(bypassed_lart_path.c_str()) == 0);
    TEST_ASSERT(bypassed_lart_sfont != FLUID_FAILED);

    /* A later, inapplicable list must not alter an earlier articulation.
       CDL is allowed both before and after ART (DLS 2.2 section 2.6). */
    const std::string articulation_path = "test_dls_conditional_articulation_tmp.dls";
    write_dls_file(articulation_path.c_str(), data);
    const auto original_attenuations = playing_voice_attenuations(articulation_path.c_str());
    TEST_ASSERT(!original_attenuations.empty());
    for(bool condition_first : { false, true })
    {
        std::vector<uint8_t> skipped_articulation(data);
        const size_t skipped_art = append_conditional_region_articulation(
            skipped_articulation, false_cdl, condition_first);
        write_dls_file(articulation_path.c_str(), skipped_articulation);
        TEST_ASSERT(playing_voice_attenuations(articulation_path.c_str()) == original_attenuations);

        // A false CDL also guards ART payloads the device cannot parse.
        write_u32(skipped_articulation, skipped_art + 8, 4); // Invalid ART cbSize
        write_dls_file(articulation_path.c_str(), skipped_articulation);
        TEST_ASSERT(playing_voice_attenuations(articulation_path.c_str()) == original_attenuations);

        std::vector<uint8_t> true_cdl(false_cdl);
        write_u32(true_cdl, 2, 1);
        std::vector<uint8_t> applied_articulation(data);
        append_conditional_region_articulation(applied_articulation, true_cdl, condition_first);
        write_dls_file(articulation_path.c_str(), applied_articulation);
        const auto applied_attenuations = playing_voice_attenuations(articulation_path.c_str());
        TEST_ASSERT(applied_attenuations.size() == original_attenuations.size());
        TEST_ASSERT(applied_attenuations != original_attenuations);
        TEST_ASSERT(applied_attenuations.front() == 960.0f);
    }

    /* Invalid conditions remain parse errors, rather than silently bypassing. */
    std::vector<uint8_t> invalid_cdl(2, 0xff);
    std::vector<uint8_t> invalid_condition(data);
    append_conditional_region_articulation(invalid_condition, invalid_cdl, false);
    write_dls_file(articulation_path.c_str(), invalid_condition);
    settings = new_fluid_settings();
    TEST_ASSERT(settings != nullptr);
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != nullptr);
    TEST_ASSERT(fluid_synth_sfload(synth, articulation_path.c_str(), 1) == FLUID_FAILED);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    TEST_ASSERT(std::remove(articulation_path.c_str()) == 0);

    /* QUERY must leave its numeric value on the CDL stack for the comparison. */
    std::vector<uint8_t> query_cdl(26);
    write_u16(query_cdl, 0, 0x0011); /* DLS_CDL_QUERY */
    static const uint8_t manufacturer_id[16] = {
        0x81, 0x11, 0x3e, 0xb0, 0x95, 0x80, 0xd2, 0x11,
        0xa1, 0xef, 0x00, 0x60, 0x08, 0x33, 0xdb, 0xd8
    };
    std::memcpy(query_cdl.data() + 2, manufacturer_id, sizeof(manufacturer_id));
    write_u16(query_cdl, 18, 0x0010); /* DLS_CDL_CONST */
    write_u32(query_cdl, 20, 0x4000);
    write_u16(query_cdl, 24, 0x000e); /* DLS_CDL_EQ */

    std::vector<uint8_t> query_dls(data);
    append_first_region_cdl(query_dls, query_cdl);
    const std::string query_path = "test_dls_query_tmp.dls";
    write_dls_file(query_path.c_str(), query_dls);

    std::vector<uint8_t> always_true_cdl(6);
    write_u16(always_true_cdl, 0, 0x0010); /* DLS_CDL_CONST */
    write_u32(always_true_cdl, 2, 1);
    std::vector<uint8_t> control_dls(data);
    append_first_region_cdl(control_dls, always_true_cdl);
    const std::string control_path = "test_dls_query_control_tmp.dls";
    write_dls_file(control_path.c_str(), control_dls);

    const size_t query_voice_count = playing_voice_attenuations(query_path.c_str()).size();
    const size_t control_voice_count = playing_voice_attenuations(control_path.c_str()).size();
    TEST_ASSERT(std::remove(query_path.c_str()) == 0);
    TEST_ASSERT(std::remove(control_path.c_str()) == 0);
    TEST_ASSERT(control_voice_count > 0);
    TEST_ASSERT(query_voice_count == control_voice_count); /* supported ManufacturersID is 0x4000 */

    /* An unsupported QUERY evaluates to FALSE, skipping just this region. */
    std::vector<uint8_t> unsupported_query(18, 0);
    write_u16(unsupported_query, 0, 0x0011); // DLS_CDL_QUERY, unknown all-zero DLSID
    std::vector<uint8_t> unsupported_query_dls(data);
    append_first_region_cdl(unsupported_query_dls, unsupported_query);
    write_dls_file(query_path.c_str(), unsupported_query_dls);
    const size_t unsupported_voice_count = playing_voice_attenuations(query_path.c_str()).size();
    TEST_ASSERT(unsupported_voice_count + 1 == control_voice_count);
    TEST_ASSERT(unsupported_voice_count > 0);
    TEST_ASSERT(std::remove(query_path.c_str()) == 0);

    /* Boolean results are 0 or UINT32_MAX, including when the input is a
     * nonzero value other than 1. Compare the result as a full 32-bit word. */
    auto assert_cdl_value = [&](std::vector<uint8_t> expression, uint32_t expected)
    {
        const size_t end = expression.size();
        expression.resize(end + 8);
        write_u16(expression, end, 0x0010); // CONST
        write_u32(expression, end + 2, expected);
        write_u16(expression, end + 6, 0x000e); // EQ
        std::vector<uint8_t> conditional_dls(data);
        append_first_region_cdl(conditional_dls, expression);
        write_dls_file(query_path.c_str(), conditional_dls);
        TEST_ASSERT(playing_voice_attenuations(query_path.c_str()).size() == control_voice_count);
        TEST_ASSERT(std::remove(query_path.c_str()) == 0);
    };

    std::vector<uint8_t> supported_query(18, 0);
    write_u16(supported_query, 0, 0x0012); // QUERY_SUPPORTED
    std::memcpy(supported_query.data() + 2, manufacturer_id, sizeof(manufacturer_id));
    assert_cdl_value(supported_query, UINT32_MAX);
    std::fill(supported_query.begin() + 2, supported_query.end(), 0);
    assert_cdl_value(supported_query, 0);

    static const uint8_t supports_dls2_id[16] = {
        0xe5, 0x99, 0x45, 0xf1, 0x89, 0x46, 0xd2, 0x11,
        0xaf, 0xa6, 0x00, 0xaa, 0x00, 0x24, 0xd8, 0xb6
    };
    write_u16(supported_query, 0, 0x0011); // QUERY returning a boolean capability
    std::memcpy(supported_query.data() + 2, supports_dls2_id, sizeof(supports_dls2_id));
    assert_cdl_value(supported_query, UINT32_MAX);

    struct BooleanCase
    {
        uint16_t opcode;
        uint32_t y; // Previous value
        uint32_t x; // Top of stack
        bool result;
    };
    const BooleanCase boolean_cases[] = {
        {0x000f, 0, 0, true}, {0x000f, 0, 2, false}, // NOT
        {0x0008, 2, 4, true}, {0x0008, 0, 2, false}, // logical AND
        {0x0009, 0, 2, true}, {0x0009, 0, 0, false}, // logical OR
        {0x000a, 4, 2, true}, {0x000a, 2, 4, false}, // LT
        {0x000b, 2, 2, true}, {0x000b, 2, 4, false}, // LE
        {0x000c, 2, 4, true}, {0x000c, 4, 2, false}, // GT
        {0x000d, 2, 2, true}, {0x000d, 4, 2, false}, // GE
        {0x000e, 2, 2, true}, {0x000e, 2, 4, false}  // EQ
    };
    for(const auto &boolean_case : boolean_cases)
    {
        const bool unary = boolean_case.opcode == 0x000f;
        const size_t operand_size = unary ? 6 : 12;
        std::vector<uint8_t> expression(operand_size + 2);
        write_u16(expression, 0, 0x0010); // CONST
        write_u32(expression, 2, unary ? boolean_case.x : boolean_case.y);
        if(!unary)
        {
            write_u16(expression, 6, 0x0010);
            write_u32(expression, 8, boolean_case.x);
        }
        write_u16(expression, operand_size, boolean_case.opcode);
        assert_cdl_value(expression, boolean_case.result ? UINT32_MAX : 0);
    }

    assert_numeric_cdl(data, control_voice_count);

    /* CDL reads must stop at the payload boundary, even when padding or a
     * following region supplies enough physical bytes to finish an operand. */
    const char *reader_bounds_path = "test_dls_reader_bounds_tmp.dls";
    auto assert_load_result = [&](const std::vector<uint8_t> &fixture, bool valid,
                                  size_t begin = 0, size_t end = 0)
    {
        write_dls_file(reader_bounds_path, fixture);
        fluid_settings_t *test_settings = new_fluid_settings();
        TEST_ASSERT(test_settings != nullptr);
        fluid_synth_t *test_synth = new_fluid_synth(test_settings);
        TEST_ASSERT(test_synth != nullptr);
        if(end != 0)
        {
            fluid_sfloader_t *loader = new_fluid_dls_loader(test_synth, test_settings);
            TEST_ASSERT(loader != nullptr);
            observed_read = loader->file_callbacks.fread;
            observed_tell = loader->file_callbacks.ftell;
            observed_begin = begin;
            observed_end = end;
            loader->file_callbacks.fread = assert_bounded_read;
            fluid_synth_add_sfloader(test_synth, loader);
        }
        const int loaded = fluid_synth_sfload(test_synth, reader_bounds_path, 1);
        delete_fluid_synth(test_synth);
        delete_fluid_settings(test_settings);
        TEST_ASSERT(std::remove(reader_bounds_path) == 0);
        TEST_ASSERT((loaded != FLUID_FAILED) == valid);
    };
    const std::vector<std::vector<uint8_t>> short_expressions = {
        {0x10},                         // Partial CONST opcode, completed by padding
        {0x10, 0x00},                   // Missing CONST operand
        {0x10, 0x00, 0x01, 0x00},       // Partial CONST operand
        {0x11, 0x00},                   // Missing QUERY identifier
        {0x12, 0x00},                   // Missing QUERY_SUPPORTED identifier
        {0x10, 0x00, 1, 0, 0, 0, 0x0f} // Partial NOT after a complete CONST
    };
    for(const auto &expression : short_expressions)
    {
        std::vector<uint8_t> short_cdl(data);
        const size_t payload = append_first_region_cdl(short_cdl, expression);
        assert_load_result(short_cdl, false, payload, payload + expression.size());
    }

    const size_t riff_end = 8 + riff_size;
    const size_t short_ptbl = find_chunk(data, 12, riff_end, "ptbl");
    TEST_ASSERT(short_ptbl < data.size());
    const uint32_t ptbl_size = read_u32(data, short_ptbl + 4);
    TEST_ASSERT(ptbl_size >= 8);
    for(uint32_t header_size : {0u, 4u})
    {
        // Keep the next RIFF sibling adjacent to the truncated header. Neither
        // cbSize nor cCues may be borrowed from that sibling's bytes.
        std::vector<uint8_t> short_header(data);
        short_header.erase(short_header.begin() + short_ptbl + 8 + header_size,
                           short_header.begin() + short_ptbl + 8 + ptbl_size);
        write_u32(short_header, short_ptbl + 4, header_size);
        write_u32(short_header, 4, riff_size - ptbl_size + header_size);
        assert_load_result(short_header, false, short_ptbl + 8, short_ptbl + 8 + header_size);
    }

    /* An INFO visitor may return after INAM; the enclosing iterator must still
     * advance past the whole LIST, including odd-sized unknown children. */
    static const uint8_t padded_info[] = {
        'L', 'I', 'S', 'T', 40, 0, 0, 0, 'I', 'N', 'F', 'O',
        'J', 'U', 'N', 'K', 3, 0, 0, 0, 1, 2, 3, 0,
        'I', 'N', 'A', 'M', 3, 0, 0, 0, 'o', 'k', 0, 0,
        'J', 'U', 'N', 'K', 3, 0, 0, 0, 4, 5, 6, 0,
        'J', 'U', 'N', 'K', 3, 0, 0, 0, 7, 8, 9, 0
    };
    std::vector<uint8_t> valid_padding(data.begin(), data.begin() + riff_end);
    valid_padding.insert(valid_padding.end(), std::begin(padded_info), std::end(padded_info));
    write_u32(valid_padding, 4, riff_size + sizeof(padded_info));
    assert_load_result(valid_padding, true);
    // A child's pad cannot be supplied by the enclosing LIST's own pad.
    // Put INAM last so the INFO visitor must inspect the truncated child.
    std::swap_ranges(valid_padding.begin() + riff_end + 24,
                     valid_padding.begin() + riff_end + 36,
                     valid_padding.begin() + riff_end + 36);
    write_u32(valid_padding, riff_end + 4, 39);
    assert_load_result(valid_padding, false);

    std::vector<uint8_t> malformed(data.begin(), data.begin() + riff_end);
    static const uint8_t invalid_info[] = {
        'L', 'I', 'S', 'T', 12, 0, 0, 0, 'I', 'N', 'F', 'O',
        'I', 'N', 'A', 'M', 8, 0, 0, 0,
        'J', 'U', 'N', 'K', 0, 0, 0, 0
    };
    malformed.insert(malformed.end(), std::begin(invalid_info), std::end(invalid_info));
    write_u32(malformed, 4, riff_size + sizeof(invalid_info));

    const std::string malformed_path = "test_dls_chunk_bounds_tmp.dls";
    std::ofstream malformed_output(malformed_path, std::ios::binary | std::ios::trunc);
    TEST_ASSERT(malformed_output.good());
    malformed_output.write(reinterpret_cast<const char *>(malformed.data()),
                           static_cast<std::streamsize>(malformed.size()));
    malformed_output.close();
    TEST_ASSERT(malformed_output.good());

    settings = new_fluid_settings();
    TEST_ASSERT(settings != nullptr);
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != nullptr);
    const int malformed_sfont = fluid_synth_sfload(synth, malformed_path.c_str(), 1);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    TEST_ASSERT(std::remove(malformed_path.c_str()) == 0);
    TEST_ASSERT(malformed_sfont == FLUID_FAILED); /* INAM data extends beyond its LIST[INFO] */

    const size_t wvpl = find_list(data, 12, riff_end, "wvpl");
    TEST_ASSERT(wvpl < data.size());
    const size_t wvpl_end = wvpl + 8 + read_u32(data, wvpl + 4);

    /* Pool cues may be reordered or repeated, and a wave may end exactly at
     * the WVPL boundary. They must not point to real waves outside the LIST,
     * or to a wave whose declared extent crosses that boundary. */
    const size_t bounds_ptbl = find_chunk(data, 12, riff_end, "ptbl");
    TEST_ASSERT(bounds_ptbl < data.size());
    const size_t bounds_cues = bounds_ptbl + 8 + read_u32(data, bounds_ptbl + 8);
    const uint32_t bounds_cue_count = read_u32(data, bounds_ptbl + 12);
    TEST_ASSERT(bounds_cue_count > 2);
    const uint32_t last_cue = read_u32(data, bounds_cues + 4 * (bounds_cue_count - 1));
    const size_t last_wave = wvpl + 12 + last_cue;
    TEST_ASSERT(last_wave + 8 + read_u32(data, last_wave + 4) == wvpl_end);
    const char *cue_bounds_path = "test_dls_pool_cue_bounds_tmp.dls";

    for(int bounds_case = 0; bounds_case < 3; ++bounds_case)
    {
        std::vector<uint8_t> cue_bounds(data);
        if(bounds_case == 0)
        {
            write_u32(cue_bounds, bounds_cues, last_cue);
            write_u32(cue_bounds, bounds_cues + 4, last_cue);
        }
        else if(bounds_case == 1)
        {
            // Existing wave chunks become top-level siblings of an empty WVPL.
            write_u32(cue_bounds, wvpl + 4, 4);
        }
        else
        {
            write_u32(cue_bounds, last_wave + 4, read_u32(data, last_wave + 4) + 2);
        }

        write_dls_file(cue_bounds_path, cue_bounds);
        settings = new_fluid_settings();
        TEST_ASSERT(settings != nullptr);
        synth = new_fluid_synth(settings);
        TEST_ASSERT(synth != nullptr);
        const int cue_bounds_sfont = fluid_synth_sfload(synth, cue_bounds_path, 1);
        TEST_ASSERT((cue_bounds_sfont != FLUID_FAILED) == (bounds_case == 0));
        delete_fluid_synth(synth);
        delete_fluid_settings(settings);
        TEST_ASSERT(std::remove(cue_bounds_path) == 0);
    }

    const size_t wave = find_list(data, wvpl + 12, wvpl_end, "wave");
    TEST_ASSERT(wave < data.size());
    const size_t wave_end = wave + 8 + read_u32(data, wave + 4);
    const size_t wsmp = find_chunk(data, wave + 12, wave_end, "wsmp");
    TEST_ASSERT(wsmp < data.size());
    TEST_ASSERT(read_u32(data, wsmp + 4) == 36);
    const size_t wsmp_payload = wsmp + 8;
    const size_t data_chunk = wsmp_payload + 36;
    TEST_ASSERT(chunk_is(data, data_chunk, "data"));

    std::vector<uint8_t> malformed_wsmp(data);
    /* Move the following data chunk next to a short wsmp chunk. The malicious
     * cbSize values make parse_wsmp's size calculation wrap to 24 bytes even
     * though it reads 36 bytes, consuming the next chunk header as loop data. */
    malformed_wsmp.erase(malformed_wsmp.begin() + wsmp_payload + 24,
                         malformed_wsmp.begin() + wsmp_payload + 36);
    write_u32(malformed_wsmp, wsmp + 4, 24);
    write_u32(malformed_wsmp, wvpl + 4, read_u32(malformed_wsmp, wvpl + 4) - 12);
    write_u32(malformed_wsmp, wave + 4, read_u32(malformed_wsmp, wave + 4) - 12);
    write_u32(malformed_wsmp, 4, read_u32(malformed_wsmp, 4) - 12);

    const size_t ptbl = find_chunk(malformed_wsmp,
                                   12,
                                   8 + read_u32(malformed_wsmp, 4),
                                   "ptbl");
    TEST_ASSERT(ptbl < malformed_wsmp.size());
    const uint32_t cbsize = read_u32(malformed_wsmp, ptbl + 8);
    const uint32_t cues = read_u32(malformed_wsmp, ptbl + 12);
    const size_t poolcue_start = ptbl + 8 + cbsize;
    const uint32_t wave_offset = static_cast<uint32_t>(wave - (wvpl + 12));

    for(uint32_t i = 0; i < cues; ++i)
    {
        const size_t poolcue = poolcue_start + i * sizeof(uint32_t);

        if(read_u32(malformed_wsmp, poolcue) > wave_offset)
        {
            write_u32(malformed_wsmp, poolcue, read_u32(malformed_wsmp, poolcue) - 12);
        }
    }

    write_u32(malformed_wsmp, wsmp_payload, 0xfffffff0u);
    write_u16(malformed_wsmp, wsmp_payload + 4, 60);
    write_u16(malformed_wsmp, wsmp_payload + 6, 0);
    write_u32(malformed_wsmp, wsmp_payload + 8, 0);
    write_u32(malformed_wsmp, wsmp_payload + 12, 0);
    write_u32(malformed_wsmp, wsmp_payload + 16, 1);
    write_u32(malformed_wsmp, wsmp_payload + 20, 40);

    const std::string malformed_wsmp_path = "test_dls_wsmp_size_tmp.dls";
    std::ofstream malformed_wsmp_output(malformed_wsmp_path, std::ios::binary | std::ios::trunc);
    TEST_ASSERT(malformed_wsmp_output.good());
    malformed_wsmp_output.write(reinterpret_cast<const char *>(malformed_wsmp.data()),
                                static_cast<std::streamsize>(malformed_wsmp.size()));
    malformed_wsmp_output.close();
    TEST_ASSERT(malformed_wsmp_output.good());

    settings = new_fluid_settings();
    TEST_ASSERT(settings != nullptr);
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != nullptr);
    const int malformed_wsmp_sfont = fluid_synth_sfload(synth, malformed_wsmp_path.c_str(), 1);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    TEST_ASSERT(std::remove(malformed_wsmp_path.c_str()) == 0);
    TEST_ASSERT(malformed_wsmp_sfont == FLUID_FAILED); /* wsmp cbSize sums overflow uint32_t */

    std::vector<uint8_t> invalid_wsmp_note(data);
    const size_t lins_with_invalid_wsmp = find_list(invalid_wsmp_note, 12, riff_end, "lins");
    const size_t lins_invalid_wsmp_end = lins_with_invalid_wsmp + 8
                                         + read_u32(invalid_wsmp_note, lins_with_invalid_wsmp + 4);
    const size_t first_instrument = find_list(invalid_wsmp_note,
                                              lins_with_invalid_wsmp + 12,
                                              lins_invalid_wsmp_end,
                                              "ins ");
    TEST_ASSERT(first_instrument < invalid_wsmp_note.size());
    const size_t first_instrument_end = first_instrument + 8
                                        + read_u32(invalid_wsmp_note, first_instrument + 4);
    const size_t first_lrgn = find_list(invalid_wsmp_note,
                                       first_instrument + 12,
                                       first_instrument_end,
                                       "lrgn");
    TEST_ASSERT(first_lrgn < invalid_wsmp_note.size());
    const size_t first_lrgn_end = first_lrgn + 8 + read_u32(invalid_wsmp_note, first_lrgn + 4);
    const size_t first_region = find_list(invalid_wsmp_note, first_lrgn + 12, first_lrgn_end, "rgn2");
    TEST_ASSERT(first_region < invalid_wsmp_note.size());
    const size_t first_region_end = first_region + 8 + read_u32(invalid_wsmp_note, first_region + 4);
    const size_t region_wsmp = find_chunk(invalid_wsmp_note,
                                         first_region + 12,
                                         first_region_end,
                                         "wsmp");
    TEST_ASSERT(region_wsmp < invalid_wsmp_note.size());

    /* WSMP owns the whole payload: declared header/loop extensions are legal,
     * undeclared trailing bytes are not. Retain Crystal's small-cbSize quirk. */
    const char *wsmp_layout_path = "test_dls_wsmp_layout_tmp.dls";
    for(int layout = 0; layout < 3; ++layout)
    {
        std::vector<uint8_t> layout_data(data);
        const uint32_t growth = layout == 0 ? 8 : (layout == 1 ? 2 : 0);
        if(layout == 0)
        {
            layout_data.insert(layout_data.begin() + region_wsmp + 28, 4, 0x5a);
            layout_data.insert(layout_data.begin() + region_wsmp + 48, 4, 0xa5);
            write_u32(layout_data, region_wsmp + 8, 24);  // Header extension
            write_u32(layout_data, region_wsmp + 32, 20); // Loop extension
        }
        else if(layout == 1)
        {
            layout_data.insert(layout_data.begin() + region_wsmp + 44, 2, 0);
        }
        else
        {
            write_u32(layout_data, region_wsmp + 8, 0);
            write_u32(layout_data, region_wsmp + 28, 0);
        }
        for(size_t size_field : { region_wsmp + 4, first_region + 4, first_lrgn + 4,
                                  first_instrument + 4, lins_with_invalid_wsmp + 4, size_t{4} })
        {
            write_u32(layout_data, size_field, read_u32(layout_data, size_field) + growth);
        }
        write_dls_file(wsmp_layout_path, layout_data);
        settings = new_fluid_settings();
        TEST_ASSERT(settings != nullptr);
        synth = new_fluid_synth(settings);
        TEST_ASSERT(synth != nullptr);
        const int layout_sfont = fluid_synth_sfload(synth, wsmp_layout_path, 1);
        TEST_ASSERT((layout_sfont != FLUID_FAILED) == (layout != 1));
        delete_fluid_synth(synth);
        delete_fluid_settings(settings);
        if(layout != 1)
        {
            TEST_ASSERT(playing_voice_attenuations(wsmp_layout_path) == original_attenuations);
        }
    }
    TEST_ASSERT(std::remove(wsmp_layout_path) == 0);

    /* lGain is signed 32-bit relative gain. Its minimum must become positive
     * attenuation, rather than overflowing during integer negation. */
    TEST_ASSERT(read_u32(data, region_wsmp + 16) == 0);
    std::vector<uint8_t> minimum_gain(data);
    write_u32(minimum_gain, region_wsmp + 16, 0x80000000u);
    const char *minimum_gain_path = "test_dls_minimum_gain_tmp.dls";
    write_dls_file(minimum_gain_path, minimum_gain);
    auto expected_attenuations = original_attenuations;
    expected_attenuations.front() += 32768.0f;
    TEST_ASSERT(playing_voice_attenuations(minimum_gain_path) == expected_attenuations);
    TEST_ASSERT(std::remove(minimum_gain_path) == 0);

    /* Wave inheritance and a region override must interpret the same WSMP
     * identically. Keep the second selected region's override unchanged, so a
     * wave gain cannot accidentally be added on top of an explicit override. */
    const size_t first_wlnk = find_chunk(data, first_region + 12, first_region_end, "wlnk");
    TEST_ASSERT(first_wlnk < data.size());
    const uint32_t sample_index = read_u32(data, first_wlnk + 16);
    TEST_ASSERT(sample_index < bounds_cue_count);
    const size_t linked_wave = wvpl + 12 + read_u32(data, bounds_cues + 4 * sample_index);
    const size_t linked_wsmp = find_chunk(data, linked_wave + 12,
                                          linked_wave + 8 + read_u32(data, linked_wave + 4), "wsmp");
    TEST_ASSERT(linked_wsmp < data.size());
    TEST_ASSERT(read_u32(data, linked_wsmp + 4) == 36);
    TEST_ASSERT(read_u32(data, region_wsmp + 4) == 36);
    struct playback_case
    {
        uint32_t gain;
        uint32_t loop_type;
        uint32_t loop_length;
        float attenuation;
        float sample_mode;
    };
    const playback_case playback_cases[] = {
        { 0, 0, 100, 0.0f, 1.0f },
        { static_cast<uint32_t>(-96 * 65536), 1, 100, 96.0f, 3.0f },
        { 0x80000000u, 99, 100, 32768.0f, 3.0f },
        { static_cast<uint32_t>(-96 * 65536), 1, 0, 96.0f, 0.0f }
    };
    const char *playback_path = "test_dls_normalized_playback_tmp.dls";
    for(const auto &test : playback_cases)
    {
        std::vector<uint8_t> playback(data);
        write_u32(playback, region_wsmp + 16, test.gain);
        write_u32(playback, region_wsmp + 32, test.loop_type);
        write_u32(playback, region_wsmp + 40, test.loop_length);
        std::copy(playback.begin() + region_wsmp + 8,
                  playback.begin() + region_wsmp + 44,
                  playback.begin() + linked_wsmp + 8);
        write_dls_file(playback_path, playback);
        auto expected = original_attenuations;
        expected.front() += test.attenuation;
        TEST_ASSERT(playing_voice_attenuations(playback_path) == expected);
        const auto override_modes = playing_voice_generators(playback_path, GEN_SAMPLEMODE);
        TEST_ASSERT(!override_modes.empty());
        TEST_ASSERT(override_modes.front() == test.sample_mode);

        // An unknown chunk is skipped without moving any pool-table offsets.
        std::memcpy(playback.data() + region_wsmp, "JUNK", 4);
        write_dls_file(playback_path, playback);
        TEST_ASSERT(playing_voice_attenuations(playback_path) == expected);
        TEST_ASSERT(playing_voice_generators(playback_path, GEN_SAMPLEMODE) == override_modes);
    }
    TEST_ASSERT(std::remove(playback_path) == 0);

    write_u16(invalid_wsmp_note, region_wsmp + 12, 255);

    const std::string invalid_wsmp_note_path = "test_dls_unity_note_tmp.dls";
    std::ofstream invalid_wsmp_note_output(invalid_wsmp_note_path, std::ios::binary | std::ios::trunc);
    TEST_ASSERT(invalid_wsmp_note_output.good());
    invalid_wsmp_note_output.write(reinterpret_cast<const char *>(invalid_wsmp_note.data()),
                                   static_cast<std::streamsize>(invalid_wsmp_note.size()));
    invalid_wsmp_note_output.close();
    TEST_ASSERT(invalid_wsmp_note_output.good());

    settings = new_fluid_settings();
    TEST_ASSERT(settings != nullptr);
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != nullptr);
    const int invalid_wsmp_note_sfont = fluid_synth_sfload(synth, invalid_wsmp_note_path.c_str(), 1);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    TEST_ASSERT(std::remove(invalid_wsmp_note_path.c_str()) == 0);
    TEST_ASSERT(invalid_wsmp_note_sfont == FLUID_FAILED); /* unity note is not a MIDI note */

    const size_t first_wave = find_list(data, wvpl + 12, wvpl_end, "wave");
    const size_t first_wave_end = first_wave + 8 + read_u32(data, first_wave + 4);
    const size_t fmt = find_chunk(data, first_wave + 12, first_wave_end, "fmt ");
    const size_t fmt_end = fmt + 8 + read_u32(data, fmt + 4)
                           + (read_u32(data, fmt + 4) & 1u);
    const size_t old_data_chunk = find_chunk(data, fmt_end, first_wave_end, "data");
    TEST_ASSERT(fmt < data.size() && old_data_chunk < data.size());
    const size_t old_data_end = old_data_chunk + 8 + read_u32(data, old_data_chunk + 4)
                                + (read_u32(data, old_data_chunk + 4) & 1u);

    std::vector<uint8_t> malformed_fmt(data.begin(), data.begin() + fmt + 8);
    write_u32(malformed_fmt, fmt + 4, 0); /* no bytes are claimed by fmt */

    std::vector<uint8_t> hidden_fmt_payload(22076, 0);
    write_u16(hidden_fmt_payload, 0, 1);      /* PCM, read past the empty fmt chunk */
    write_u16(hidden_fmt_payload, 2, 1);      /* mono */
    write_u32(hidden_fmt_payload, 4, 22050);  /* becomes the next fake chunk's size */
    write_u32(hidden_fmt_payload, 8, 22050);  /* avgBytesPerSec */
    write_u16(hidden_fmt_payload, 12, 1);     /* blockAlign */
    write_u16(hidden_fmt_payload, 14, 8);     /* bitsPerSample */
    const size_t replacement_data = 22066;
    std::memcpy(hidden_fmt_payload.data() + replacement_data, "data", 4);
    write_u32(hidden_fmt_payload, replacement_data + 4, 2);
    hidden_fmt_payload[replacement_data + 8] = 0;
    hidden_fmt_payload[replacement_data + 9] = 0;
    malformed_fmt.insert(malformed_fmt.end(), hidden_fmt_payload.begin(), hidden_fmt_payload.end());
    malformed_fmt.insert(malformed_fmt.end(), data.begin() + old_data_end, data.end());

    const uint32_t growth = static_cast<uint32_t>(malformed_fmt.size() - data.size());
    write_u32(malformed_fmt, first_wave + 4, read_u32(data, first_wave + 4) + growth);
    write_u32(malformed_fmt, wvpl + 4, read_u32(data, wvpl + 4) + growth);
    write_u32(malformed_fmt, 4, read_u32(data, 4) + growth);

    const size_t fmt_poolcue_start = ptbl + 8 + read_u32(data, ptbl + 8);
    for(uint32_t i = 0; i < cues; ++i)
    {
        const size_t poolcue = fmt_poolcue_start + i * sizeof(uint32_t);
        const uint32_t value = read_u32(data, poolcue);

        if(value > 0)
        {
            write_u32(malformed_fmt, poolcue, value + growth);
        }
    }

    const std::string malformed_fmt_path = "test_dls_fmt_bounds_tmp.dls";
    std::ofstream malformed_fmt_output(malformed_fmt_path, std::ios::binary | std::ios::trunc);
    TEST_ASSERT(malformed_fmt_output.good());
    malformed_fmt_output.write(reinterpret_cast<const char *>(malformed_fmt.data()),
                               static_cast<std::streamsize>(malformed_fmt.size()));
    malformed_fmt_output.close();
    TEST_ASSERT(malformed_fmt_output.good());

    settings = new_fluid_settings();
    TEST_ASSERT(settings != nullptr);
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != nullptr);
    const int malformed_fmt_sfont = fluid_synth_sfload(synth, malformed_fmt_path.c_str(), 1);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    TEST_ASSERT(std::remove(malformed_fmt_path.c_str()) == 0);
    TEST_ASSERT(malformed_fmt_sfont == FLUID_FAILED); /* empty fmt is read from sibling bytes */

    /* A malicious cRegions count must not allocate beyond the instrument's encoded size. */
    std::vector<uint8_t> excessive_region_count(data);
    const size_t lins_for_count = find_list(excessive_region_count, 12, riff_end, "lins");
    TEST_ASSERT(lins_for_count < excessive_region_count.size());
    const size_t lins_count_end = lins_for_count + 8 + read_u32(excessive_region_count, lins_for_count + 4);
    const size_t instrument_for_count = find_list(excessive_region_count,
                                                 lins_for_count + 12,
                                                 lins_count_end,
                                                 "ins ");
    TEST_ASSERT(instrument_for_count < excessive_region_count.size());
    const size_t instrument_end = instrument_for_count + 8
                                  + read_u32(excessive_region_count, instrument_for_count + 4);
    const size_t insh_for_count = find_chunk(excessive_region_count,
                                            instrument_for_count + 12,
                                            instrument_end,
                                            "insh");
    TEST_ASSERT(insh_for_count < excessive_region_count.size());
    write_u32(excessive_region_count, insh_for_count + 8, 100'000'000);

    const std::string excessive_region_path = "test_dls_excessive_region_count_tmp.dls";
    std::ofstream excessive_region_output(excessive_region_path, std::ios::binary | std::ios::trunc);
    TEST_ASSERT(excessive_region_output.good());
    excessive_region_output.write(reinterpret_cast<const char *>(excessive_region_count.data()),
                                  static_cast<std::streamsize>(excessive_region_count.size()));
    excessive_region_output.close();
    TEST_ASSERT(excessive_region_output.good());

    settings = new_fluid_settings();
    TEST_ASSERT(settings != nullptr);
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != nullptr);
    const int excessive_region_sfont = fluid_synth_sfload(synth, excessive_region_path.c_str(), 1);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    TEST_ASSERT(std::remove(excessive_region_path.c_str()) == 0);
    TEST_ASSERT(excessive_region_sfont == FLUID_FAILED);

    std::vector<uint8_t> excessive_instrument_count(data);
    const size_t colh = find_chunk(excessive_instrument_count, 12, riff_end, "colh");
    TEST_ASSERT(colh < excessive_instrument_count.size());
    write_u32(excessive_instrument_count, colh + 8, 100'000'000);

    const std::string excessive_instrument_path = "test_dls_excessive_instrument_count_tmp.dls";
    std::ofstream excessive_instrument_output(excessive_instrument_path,
                                              std::ios::binary | std::ios::trunc);
    TEST_ASSERT(excessive_instrument_output.good());
    excessive_instrument_output.write(reinterpret_cast<const char *>(excessive_instrument_count.data()),
                                      static_cast<std::streamsize>(excessive_instrument_count.size()));
    excessive_instrument_output.close();
    TEST_ASSERT(excessive_instrument_output.good());

    settings = new_fluid_settings();
    TEST_ASSERT(settings != nullptr);
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != nullptr);
    const int excessive_instrument_sfont = fluid_synth_sfload(synth, excessive_instrument_path.c_str(), 1);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    TEST_ASSERT(std::remove(excessive_instrument_path.c_str()) == 0);
    TEST_ASSERT(excessive_instrument_sfont == FLUID_FAILED);

    /* A PGAL alias count must fit inside its chunk instead of reading the next sibling. */
    const size_t lins_for_pgal = find_list(data, 12, riff_end, "lins");
    TEST_ASSERT(lins_for_pgal < data.size());
    std::vector<uint8_t> malformed_pgal(data);
    std::vector<uint8_t> pgal_chunk(144, 0);
    std::memcpy(pgal_chunk.data(), "pgal", 4);
    write_u32(pgal_chunk, 4, 136);
    write_u32(pgal_chunk, 140, 1); /* One alias record is declared, but none is stored. */
    malformed_pgal.insert(malformed_pgal.begin() + lins_for_pgal,
                          pgal_chunk.begin(), pgal_chunk.end());
    write_u32(malformed_pgal, 4, read_u32(malformed_pgal, 4) + pgal_chunk.size());

    const std::string malformed_pgal_path = "test_dls_pgal_bounds_tmp.dls";
    std::ofstream malformed_pgal_output(malformed_pgal_path, std::ios::binary | std::ios::trunc);
    TEST_ASSERT(malformed_pgal_output.good());
    malformed_pgal_output.write(reinterpret_cast<const char *>(malformed_pgal.data()),
                                 static_cast<std::streamsize>(malformed_pgal.size()));
    malformed_pgal_output.close();
    TEST_ASSERT(malformed_pgal_output.good());

    settings = new_fluid_settings();
    TEST_ASSERT(settings != nullptr);
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != nullptr);
    const int malformed_pgal_sfont = fluid_synth_sfload(synth, malformed_pgal_path.c_str(), 1);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    TEST_ASSERT(std::remove(malformed_pgal_path.c_str()) == 0);
    TEST_ASSERT(malformed_pgal_sfont == FLUID_FAILED);

    /* A zero-sized RGNH must not make parse_rgn treat its bytes as a new WLNK. */
    const size_t lins_for_rgnh = find_list(data, 12, riff_end, "lins");
    TEST_ASSERT(lins_for_rgnh < data.size());
    const size_t lins_rgnh_end = lins_for_rgnh + 8 + read_u32(data, lins_for_rgnh + 4);
    const size_t instrument_for_rgnh = find_list(data, lins_for_rgnh + 12, lins_rgnh_end, "ins ");
    TEST_ASSERT(instrument_for_rgnh < data.size());
    const size_t instrument_rgnh_end = instrument_for_rgnh + 8 + read_u32(data, instrument_for_rgnh + 4);
    const size_t lrgn_for_rgnh = find_list(data, instrument_for_rgnh + 12, instrument_rgnh_end, "lrgn");
    TEST_ASSERT(lrgn_for_rgnh < data.size());
    const size_t lrgn_end = lrgn_for_rgnh + 8 + read_u32(data, lrgn_for_rgnh + 4);
    const size_t region_for_rgnh = find_list(data, lrgn_for_rgnh + 12, lrgn_end, "rgn2");
    TEST_ASSERT(region_for_rgnh < data.size());
    const size_t region_end = region_for_rgnh + 8 + read_u32(data, region_for_rgnh + 4);
    const size_t rgnh = find_chunk(data, region_for_rgnh + 12, region_end, "rgnh");
    TEST_ASSERT(rgnh < data.size());

    std::vector<uint8_t> malformed_rgnh(data);
    write_u32(malformed_rgnh, rgnh + 4, 0);
    /* This unknown child spans the remaining RGNH payload, keeping the WLNK
       aligned while its header is also read as RGNH data. */
    write_u32(malformed_rgnh, rgnh + 8, 0x007f0000u);
    write_u32(malformed_rgnh, rgnh + 12, 4);

    const std::string malformed_rgnh_path = "test_dls_rgnh_bounds_tmp.dls";
    std::ofstream malformed_rgnh_output(malformed_rgnh_path, std::ios::binary | std::ios::trunc);
    TEST_ASSERT(malformed_rgnh_output.good());
    malformed_rgnh_output.write(reinterpret_cast<const char *>(malformed_rgnh.data()),
                                 static_cast<std::streamsize>(malformed_rgnh.size()));
    malformed_rgnh_output.close();
    TEST_ASSERT(malformed_rgnh_output.good());

    settings = new_fluid_settings();
    TEST_ASSERT(settings != nullptr);
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != nullptr);
    const int malformed_rgnh_sfont = fluid_synth_sfload(synth, malformed_rgnh_path.c_str(), 1);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    TEST_ASSERT(std::remove(malformed_rgnh_path.c_str()) == 0);
    TEST_ASSERT(malformed_rgnh_sfont == FLUID_FAILED);

    /* A DLS key-number scale can derive an internal voice key above 127. */
    std::vector<uint8_t> large_keynum_scale(data);
    const size_t lins = find_list(large_keynum_scale, 12, riff_end, "lins");
    TEST_ASSERT(lins < large_keynum_scale.size());
    const size_t lins_end = lins + 8 + read_u32(large_keynum_scale, lins + 4);
    const size_t instrument = find_list(large_keynum_scale, lins + 12, lins_end, "ins ");
    TEST_ASSERT(instrument < large_keynum_scale.size());
    const size_t insh = find_chunk(large_keynum_scale, instrument + 12,
                                   instrument + 8 + read_u32(large_keynum_scale, instrument + 4), "insh");
    TEST_ASSERT(insh < large_keynum_scale.size());

    /* Add instrument-level LART: CONN_SRC_KEYNUMBER -> CONN_DST_KEYNUMBER, scale 2.2. */
    std::vector<uint8_t> keynum_art(40, 0);
    std::memcpy(keynum_art.data(), "LIST", 4);
    write_u32(keynum_art, 4, 32);
    std::memcpy(keynum_art.data() + 8, "lart", 4);
    std::memcpy(keynum_art.data() + 12, "art2", 4);
    write_u32(keynum_art, 16, 20);
    write_u32(keynum_art, 20, 8);
    write_u32(keynum_art, 24, 1);
    write_u16(keynum_art, 28, 3);
    write_u16(keynum_art, 30, 0);
    write_u16(keynum_art, 32, 5);
    write_u16(keynum_art, 34, 0);
    write_u32(keynum_art, 36, 1845493760u);

    const size_t insh_end = insh + 8 + read_u32(large_keynum_scale, insh + 4)
                            + (read_u32(large_keynum_scale, insh + 4) & 1u);
    /* A single instrument LART must project its final key-number scale.
     * Use ordinary notes and a scale of 0.5, without custom tuning or clamps. */
    {
        std::vector<uint8_t> single_keynum_scale(data);
        std::vector<uint8_t> single_keynum_art(keynum_art);
        write_u32(single_keynum_art, 36, 419430400u); // 0.5 * 12800 * 65536
        single_keynum_scale.insert(single_keynum_scale.begin() + insh_end,
                                    single_keynum_art.begin(), single_keynum_art.end());
        for(size_t size_field : { instrument + 4, lins + 4, size_t{4} })
        {
            write_u32(single_keynum_scale, size_field,
                      read_u32(data, size_field) + single_keynum_art.size());
        }

        const char *single_keynum_path = "test_dls_single_keynum_scale_tmp.dls";
        write_dls_file(single_keynum_path, single_keynum_scale);
        settings = new_fluid_settings();
        TEST_ASSERT(settings != nullptr);
        synth = new_fluid_synth(settings);
        TEST_ASSERT(synth != nullptr);
        const int single_keynum_sfont = fluid_synth_sfload(synth, single_keynum_path, 1);
        TEST_ASSERT(single_keynum_sfont >= 0);
        TEST_SUCCESS(fluid_synth_program_select(synth, 0, single_keynum_sfont, 0, 0));
        assert_dls_keynum_overrides(synth, 30);
        assert_dls_note_lifecycle(synth, 0, 30);

        /* Mono retrigger must resolve the new note, not reuse stale KEYNUM. */
        assert_dls_mono_lifecycle(synth, 0, 30, 32);
        assert_dls_retrigger_lifecycle(synth);
        assert_dls_pressure_and_reuse(synth);
        delete_fluid_synth(synth);
        delete_fluid_settings(settings);


        /* Active stealing has its own synth: no prior mode or stopped slots. */
        write_dls_file(single_keynum_path, single_keynum_scale);
        settings = new_fluid_settings();
        TEST_ASSERT(settings != nullptr);
        TEST_SUCCESS(fluid_settings_setint(settings, "synth.reverb.active", 0));
        TEST_SUCCESS(fluid_settings_setint(settings, "synth.chorus.active", 0));
        synth = new_fluid_synth(settings);
        TEST_ASSERT(synth != nullptr);
        const int stealing_sfont = fluid_synth_sfload(synth, single_keynum_path, 1);
        TEST_ASSERT(stealing_sfont >= 0);
        TEST_SUCCESS(fluid_synth_program_select(synth, 0, stealing_sfont, 0, 0));
        assert_dls_active_steal(synth);
        delete_fluid_synth(synth);
        delete_fluid_settings(settings);

        /* Region-owned articulation takes precedence over instrument mods.
         * Add CC1 only to the first region; the sibling is an unchanged control. */
        std::vector<uint8_t> modulated_keynum_scale(single_keynum_scale);
        std::vector<uint8_t> applicable_cdl(6, 0);
        write_u16(applicable_cdl, 0, 0x0010); // CONST true
        write_u32(applicable_cdl, 2, 1);
        const size_t mod_art = append_conditional_region_articulation(
                                  modulated_keynum_scale, applicable_cdl, false);
        write_u16(modulated_keynum_scale, mod_art + 16, 0x0081); // CC1
        write_u16(modulated_keynum_scale, mod_art + 20, 3); // pitch in cents
        write_u32(modulated_keynum_scale, mod_art + 24, 1200u * 65536u);
        write_dls_file(single_keynum_path, modulated_keynum_scale);
        settings = new_fluid_settings();
        TEST_ASSERT(settings != nullptr);
        synth = new_fluid_synth(settings);
        TEST_ASSERT(synth != nullptr);
        const int modulated_sfont = fluid_synth_sfload(synth, single_keynum_path, 1);
        TEST_ASSERT(modulated_sfont >= 0);
        TEST_SUCCESS(fluid_synth_program_select(synth, 0, modulated_sfont, 0, 0));
        assert_dls_articulation_modulator(synth);
        delete_fluid_synth(synth);
        delete_fluid_settings(settings);

        /* Ordinary identity mapping keeps both keys equal and releases normally. */
        write_dls_file(single_keynum_path, data);
        settings = new_fluid_settings();
        TEST_ASSERT(settings != nullptr);
        synth = new_fluid_synth(settings);
        TEST_ASSERT(synth != nullptr);
        const int identity_sfont = fluid_synth_sfload(synth, single_keynum_path, 1);
        TEST_ASSERT(identity_sfont >= 0);
        TEST_SUCCESS(fluid_synth_program_select(synth, 0, identity_sfont, 0, 0));
        assert_dls_keynum_overrides(synth, 60);
        assert_dls_note_lifecycle(synth, 0, 60);
        assert_dls_mono_lifecycle(synth, 0, 60, 64);
        delete_fluid_synth(synth);
        delete_fluid_settings(settings);

        /* Valid PGAL drum alias: only ordinary note 60 maps to note 30. */
        std::vector<uint8_t> aliased_drum(data);
        write_u32(aliased_drum, insh + 12, 0x80000000u);
        std::vector<uint8_t> alias_chunk(144, 0);
        std::memcpy(alias_chunk.data(), "pgal", 4);
        write_u32(alias_chunk, 4, 136); // Version, 128 aliases, zero instrument aliases
        for(size_t note = 0; note < 128; ++note)
        {
            alias_chunk[12 + note] = static_cast<uint8_t>(note);
        }
        alias_chunk[12 + 60] = 30;
        aliased_drum.insert(aliased_drum.begin() + riff_end,
                            alias_chunk.begin(), alias_chunk.end());
        write_u32(aliased_drum, 4, read_u32(data, 4) + alias_chunk.size());
        write_dls_file(single_keynum_path, aliased_drum);
        settings = new_fluid_settings();
        TEST_ASSERT(settings != nullptr);
        synth = new_fluid_synth(settings);
        TEST_ASSERT(synth != nullptr);
        const int alias_sfont = fluid_synth_sfload(synth, single_keynum_path, 1);
        TEST_ASSERT(alias_sfont >= 0);
        /* Use the basic channel so mono-mode CC126 is valid for this drum test. */
        TEST_SUCCESS(fluid_synth_set_channel_type(synth, 0, CHANNEL_TYPE_DRUM));
        TEST_SUCCESS(fluid_synth_program_select(synth, 0, alias_sfont, 128, 0));
        assert_dls_note_lifecycle(synth, 0, 30);
        assert_dls_mono_lifecycle(synth, 0, 30, 64);
        delete_fluid_synth(synth);
        delete_fluid_settings(settings);

        assert_unmapped_preset_paths(data, insh, alias_chunk, single_keynum_path);

        TEST_ASSERT(std::remove(single_keynum_path) == 0);
    }

    std::vector<uint8_t> keynum_arts(keynum_art);
    keynum_arts.insert(keynum_arts.end(), keynum_art.begin(), keynum_art.end());
    large_keynum_scale.insert(large_keynum_scale.begin() + insh_end,
                              keynum_arts.begin(), keynum_arts.end());
    write_u32(large_keynum_scale, instrument + 4, read_u32(data, instrument + 4) + keynum_arts.size());
    write_u32(large_keynum_scale, lins + 4, read_u32(data, lins + 4) + keynum_arts.size());
    write_u32(large_keynum_scale, 4, read_u32(data, 4) + keynum_arts.size());

    const std::string large_keynum_path = "test_dls_keynum_scale_tmp.dls";
    std::ofstream large_keynum_output(large_keynum_path, std::ios::binary | std::ios::trunc);
    TEST_ASSERT(large_keynum_output.good());
    large_keynum_output.write(reinterpret_cast<const char *>(large_keynum_scale.data()),
                              static_cast<std::streamsize>(large_keynum_scale.size()));
    large_keynum_output.close();
    TEST_ASSERT(large_keynum_output.good());

    settings = new_fluid_settings();
    TEST_ASSERT(settings != nullptr);
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != nullptr);
    const int keynum_sfont = fluid_synth_sfload(synth, large_keynum_path.c_str(), 1);
    TEST_ASSERT(keynum_sfont >= 0);
    TEST_SUCCESS(fluid_synth_program_select(synth, 0, keynum_sfont, 0, 0));
    double pitches[128];
    for(int i = 0; i < 128; ++i)
    {
        pitches[i] = i * 100.0;
    }
    pitches[60] = 0.0; /* keep sample selection in range while the voice key scales */
    TEST_SUCCESS(fluid_synth_activate_key_tuning(synth, 0, 1, "DLS key test", pitches, 0));
    TEST_SUCCESS(fluid_synth_activate_tuning(synth, 0, 0, 1, 1));
    TEST_SUCCESS(fluid_synth_noteon(synth, 0, 60, 100));
    fluid_voice_t *melodic_voices[512] = {};
    fluid_synth_get_voicelist(synth, melodic_voices, 512, -1);
    size_t melodic_voice_count = 0;
    for(fluid_voice_t *voice : melodic_voices)
    {
        if(voice != nullptr && fluid_voice_is_playing(voice))
        {
            TEST_ASSERT(fluid_voice_get_key(voice) == 60);
            TEST_ASSERT(fluid_voice_get_actual_key(voice) == 127);
            melodic_voice_count++;
        }
    }
    /* The tuned selection key is zero, selecting the two low-key regions.
     * Neither the scaled key (132) nor clamped voice key (127) selects them. */
    TEST_ASSERT(melodic_voice_count == 2);
    float audio[256 * 2];
    TEST_SUCCESS(fluid_synth_write_float(synth, 256, audio, 0, 2, audio, 1, 2));
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    TEST_ASSERT(std::remove(large_keynum_path.c_str()) == 0);

    /* The same voice-key bound applies to drums. Malformed region ranges can
     * otherwise select a scaled key above 127, which indexes a tuning table. */
    write_u32(large_keynum_scale, insh + 12, 0x80000000u); // Drum instrument
    const size_t drum_lrgn = find_list(large_keynum_scale, instrument + 12,
                                       instrument + 8 + read_u32(large_keynum_scale, instrument + 4),
                                       "lrgn");
    TEST_ASSERT(drum_lrgn < large_keynum_scale.size());
    const size_t drum_region = find_list(large_keynum_scale, drum_lrgn + 12,
                                         drum_lrgn + 8 + read_u32(large_keynum_scale, drum_lrgn + 4),
                                         "rgn2");
    TEST_ASSERT(drum_region < large_keynum_scale.size());
    const size_t drum_rgnh = find_chunk(large_keynum_scale, drum_region + 12,
                                        drum_region + 8 + read_u32(large_keynum_scale, drum_region + 4),
                                        "rgnh");
    TEST_ASSERT(drum_rgnh < large_keynum_scale.size());
    write_u16(large_keynum_scale, drum_rgnh + 8, 0); // key low
    write_u16(large_keynum_scale, drum_rgnh + 10, 65535); // Invalid key high admits 132
    write_dls_file(large_keynum_path.c_str(), large_keynum_scale);

    settings = new_fluid_settings();
    TEST_ASSERT(settings != nullptr);
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != nullptr);
    const int drum_keynum_sfont = fluid_synth_sfload(synth, large_keynum_path.c_str(), 1);
    TEST_ASSERT(drum_keynum_sfont >= 0);
    TEST_SUCCESS(fluid_synth_program_select(synth, 9, drum_keynum_sfont, 128, 0));
    TEST_SUCCESS(fluid_synth_activate_key_tuning(synth, 0, 1, "DLS drum key test", pitches, 0));
    TEST_SUCCESS(fluid_synth_activate_tuning(synth, 9, 0, 1, 1));
    TEST_SUCCESS(fluid_synth_noteon(synth, 9, 60, 100));
    fluid_voice_t *drum_voices[512] = {};
    fluid_synth_get_voicelist(synth, drum_voices, 512, -1);
    TEST_ASSERT(drum_voices[0] != nullptr);
    TEST_ASSERT(fluid_voice_get_key(drum_voices[0]) == 60);
    TEST_ASSERT(fluid_voice_get_actual_key(drum_voices[0]) == 127);
    TEST_SUCCESS(fluid_synth_write_float(synth, 256, audio, 0, 2, audio, 1, 2));
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    TEST_ASSERT(std::remove(large_keynum_path.c_str()) == 0);
    return EXIT_SUCCESS;
}
