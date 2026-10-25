#include "test.h"
#include "fluidsynth.h"
#include <stdint.h>
#include <string.h>

#define SF2_SAMPLE_HEADER_SIZE 46u
#define SF2_INSTRUMENT_SIZE 22u
#define SF2_BAG_SIZE 4u
#define SF2_GENERATOR_SIZE 4u

static uint16_t read_u16(const unsigned char *data)
{
    return (uint16_t)data[0] | (uint16_t)data[1] << 8;
}

static void write_u16(unsigned char *data, uint16_t value)
{
    data[0] = (unsigned char)value;
    data[1] = (unsigned char)(value >> 8);
}

static uint32_t read_u32(const unsigned char *data)
{
    return (uint32_t)data[0] | (uint32_t)data[1] << 8 | (uint32_t)data[2] << 16 | (uint32_t)data[3] << 24;
}

static int create_sfont_with_original_pitch(const char *path, unsigned char original_pitch)
{
    FILE *input = fopen(TEST_SOUNDFONT, "rb");
    FILE *output = NULL;
    unsigned char *data = NULL;
    long file_length;
    size_t file_size, offset, pdta_start = 0, pdta_end = 0;
    size_t inst_start = 0, inst_size = 0, ibag_start = 0, ibag_size = 0;
    size_t igen_start = 0, igen_size = 0, shdr_start = 0, shdr_size = 0;
    size_t first_local_bag, generator_index;
    int result = 0;

    if(input == NULL || fseek(input, 0, SEEK_END) != 0 || (file_length = ftell(input)) < 0 ||
       fseek(input, 0, SEEK_SET) != 0)
    {
        if(input != NULL)
        {
            fclose(input);
        }

        return 0;
    }

    file_size = (size_t)file_length;
    data = malloc(file_size);
    if(data == NULL || fread(data, 1, file_size, input) != file_size)
    {
        free(data);
        fclose(input);
        return 0;
    }
    fclose(input);

    for(offset = 12; offset + 12 <= file_size;)
    {
        const uint32_t chunk_size = read_u32(data + offset + 4);
        const size_t next = offset + 8 + chunk_size + (chunk_size & 1u);

        if(next > file_size)
        {
            goto cleanup;
        }

        if(memcmp(data + offset, "LIST", 4) == 0 && chunk_size >= 4 && memcmp(data + offset + 8, "pdta", 4) == 0)
        {
            pdta_start = offset + 12;
            pdta_end = offset + 8 + chunk_size;
            break;
        }

        offset = next;
    }

    if(pdta_start == 0)
    {
        goto cleanup;
    }

    for(offset = pdta_start; offset + 8 <= pdta_end;)
    {
        const uint32_t chunk_size = read_u32(data + offset + 4);
        const size_t next = offset + 8 + chunk_size + (chunk_size & 1u);

        if(next > pdta_end)
        {
            goto cleanup;
        }

        if(memcmp(data + offset, "inst", 4) == 0)
        {
            inst_start = offset + 8;
            inst_size = chunk_size;
        }
        else if(memcmp(data + offset, "ibag", 4) == 0)
        {
            ibag_start = offset + 8;
            ibag_size = chunk_size;
        }
        else if(memcmp(data + offset, "igen", 4) == 0)
        {
            igen_start = offset + 8;
            igen_size = chunk_size;
        }
        else if(memcmp(data + offset, "shdr", 4) == 0)
        {
            shdr_start = offset + 8;
            shdr_size = chunk_size;
        }

        offset = next;
    }

    if(inst_start == 0 || ibag_start == 0 || igen_start == 0 || shdr_start == 0 ||
       inst_size < 2 * SF2_INSTRUMENT_SIZE || ibag_size < 3 * SF2_BAG_SIZE ||
       igen_size < SF2_GENERATOR_SIZE || shdr_size < SF2_SAMPLE_HEADER_SIZE * 2u)
    {
        goto cleanup;
    }

    /* The first preset's first instrument zone has an explicit root-key
     * override. Remove it so playback uses the sample's original pitch. */
    first_local_bag = read_u16(data + inst_start + SF2_INSTRUMENT_SIZE + 20) - 1;
    if(first_local_bag + 1 >= ibag_size / SF2_BAG_SIZE)
    {
        goto cleanup;
    }
    generator_index = read_u16(data + ibag_start + first_local_bag * SF2_BAG_SIZE);
    if(generator_index >= igen_size / SF2_GENERATOR_SIZE ||
       read_u16(data + igen_start + generator_index * SF2_GENERATOR_SIZE) != 58)
    {
        goto cleanup;
    }
    write_u16(data + igen_start + generator_index * SF2_GENERATOR_SIZE + 2, UINT16_MAX);

    /* SF2 specifies 255 for unpitched samples; the renderer must use key 60. */
    /* The first preset's first instrument zone uses sample header 1. */
    data[shdr_start + SF2_SAMPLE_HEADER_SIZE + 40] = original_pitch;

    output = fopen(path, "wb");
    if(output != NULL)
    {
        result = fwrite(data, 1, file_size, output) == file_size;
        if(fclose(output) != 0)
        {
            result = 0;
        }
    }

cleanup:
    free(data);
    return result;
}

int main(void)
{
    static const char *const unpitched_path = "sfont-unpitched-tuning.sf2";
    static const char *const pitched_path = "sfont-pitched-tuning.sf2";
    fluid_settings_t *unpitched_settings, *pitched_settings;
    fluid_synth_t *unpitched_synth, *pitched_synth;
    double pitches[128];
    float unpitched_audio[1024], pitched_audio[1024];
    int unpitched_sfid, pitched_sfid;
    int i;

    TEST_ASSERT(create_sfont_with_original_pitch(unpitched_path, 255));
    TEST_ASSERT(create_sfont_with_original_pitch(pitched_path, 60));
    unpitched_settings = new_fluid_settings();
    pitched_settings = new_fluid_settings();
    TEST_ASSERT(unpitched_settings != NULL);
    TEST_ASSERT(pitched_settings != NULL);
    unpitched_synth = new_fluid_synth(unpitched_settings);
    pitched_synth = new_fluid_synth(pitched_settings);
    TEST_ASSERT(unpitched_synth != NULL);
    TEST_ASSERT(pitched_synth != NULL);
    unpitched_sfid = fluid_synth_sfload(unpitched_synth, unpitched_path, 1);
    pitched_sfid = fluid_synth_sfload(pitched_synth, pitched_path, 1);
    TEST_ASSERT(unpitched_sfid != FLUID_FAILED);
    TEST_ASSERT(pitched_sfid != FLUID_FAILED);
    TEST_SUCCESS(fluid_synth_program_select(unpitched_synth, 0, unpitched_sfid, 0, 127));
    TEST_SUCCESS(fluid_synth_program_select(pitched_synth, 0, pitched_sfid, 0, 127));

    for(i = 0; i < 128; ++i)
    {
        pitches[i] = i * 100.0;
    }
    TEST_SUCCESS(fluid_synth_activate_key_tuning(unpitched_synth, 0, 1, "unpitched sample", pitches, 0));
    TEST_SUCCESS(fluid_synth_activate_tuning(unpitched_synth, 0, 0, 1, 1));
    TEST_SUCCESS(fluid_synth_activate_key_tuning(pitched_synth, 0, 1, "pitched sample", pitches, 0));
    TEST_SUCCESS(fluid_synth_activate_tuning(pitched_synth, 0, 0, 1, 1));
    TEST_SUCCESS(fluid_synth_noteon(unpitched_synth, 0, 60, 100));
    TEST_SUCCESS(fluid_synth_noteon(pitched_synth, 0, 60, 100));
    TEST_SUCCESS(fluid_synth_write_float(unpitched_synth, 512,
                                         unpitched_audio, 0, 2, unpitched_audio, 1, 2));
    TEST_SUCCESS(fluid_synth_write_float(pitched_synth, 512,
                                         pitched_audio, 0, 2, pitched_audio, 1, 2));
    TEST_ASSERT(memcmp(unpitched_audio, pitched_audio, sizeof(unpitched_audio)) == 0);

    delete_fluid_synth(unpitched_synth);
    delete_fluid_synth(pitched_synth);
    delete_fluid_settings(unpitched_settings);
    delete_fluid_settings(pitched_settings);
    TEST_ASSERT(remove(unpitched_path) == 0);
    TEST_ASSERT(remove(pitched_path) == 0);
    return EXIT_SUCCESS;
}
