#include "test.h"
#include <fluidsynth.h>

#include <cstdint>
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

static void append_first_region_cdl(std::vector<uint8_t> &data,
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
    std::vector<uint8_t> chunk(8 + expression.size());
    std::memcpy(chunk.data(), "cdl ", 4);
    write_u32(chunk, 4, static_cast<uint32_t>(expression.size()));
    std::memcpy(chunk.data() + 8, expression.data(), expression.size());
    const uint32_t growth = static_cast<uint32_t>(chunk.size());
    data.insert(data.begin() + insert_at, chunk.begin(), chunk.end());

    for(size_t size_field : { region + 4, lrgn + 4, instrument + 4, lins + 4, size_t{4} })
    {
        write_u32(data, size_field, read_u32(data, size_field) + growth);
    }
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

static std::vector<float> playing_voice_attenuations(const char *path)
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
    std::vector<float> attenuations;

    for(fluid_voice_t *voice : voices)
    {
        if(voice != nullptr && fluid_voice_is_playing(voice))
        {
            attenuations.push_back(fluid_voice_gen_get(voice, GEN_ATTENUATION));
        }
    }

    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    return attenuations;
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

    const size_t riff_end = 8 + riff_size;
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
    float audio[256 * 2];
    TEST_SUCCESS(fluid_synth_write_float(synth, 256, audio, 0, 2, audio, 1, 2));
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    TEST_ASSERT(std::remove(large_keynum_path.c_str()) == 0);
    return EXIT_SUCCESS;
}
