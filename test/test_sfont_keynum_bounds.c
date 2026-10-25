#include "test.h"
#include "fluidsynth.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SF2_INSTRUMENT_SIZE 22u
#define SF2_BAG_SIZE 4u
#define SF2_GENERATOR_SIZE 4u
#define SF2_KEYNUM_GENERATOR 46u
#define SF2_OVERRIDEROOTKEY_GENERATOR 58u

static uint32_t read_u32(const unsigned char *data)
{
    return (uint32_t)data[0] | (uint32_t)data[1] << 8 | (uint32_t)data[2] << 16 | (uint32_t)data[3] << 24;
}

static uint16_t read_u16(const unsigned char *data)
{
    return (uint16_t)data[0] | (uint16_t)data[1] << 8;
}

static void write_u16(unsigned char *data, uint16_t value)
{
    data[0] = (unsigned char)value;
    data[1] = (unsigned char)(value >> 8);
}

static int voice_has_actual_key(fluid_synth_t *synth, int key, int actual_key)
{
    const int polyphony = fluid_synth_get_polyphony(synth);
    fluid_voice_t **voices = calloc((size_t)polyphony, sizeof(*voices));
    int i;
    int matched = 0;

    if(voices == NULL)
    {
        return 0;
    }

    fluid_synth_get_voicelist(synth, voices, polyphony, -1);

    for(i = 0; i < polyphony; ++i)
    {
        if(voices[i] == NULL)
        {
            break;
        }

        if(fluid_voice_get_key(voices[i]) == key)
        {
            matched = fluid_voice_get_actual_key(voices[i]) == actual_key;
            break;
        }
    }

    free(voices);
    return matched;
}

/* Make the first preset's instrument force a MIDI key outside 0..127. */
static int create_sfont_with_invalid_fixed_key(const char *path, uint16_t generator_id)
{
    FILE *input = fopen(TEST_SOUNDFONT, "rb");
    FILE *output = NULL;
    unsigned char *data = NULL;
    long file_length;
    size_t file_size;
    size_t pdta_start = 0, pdta_end = 0;
    size_t inst_start = 0, inst_size = 0;
    size_t ibag_start = 0, ibag_size = 0;
    size_t igen_start = 0, igen_size = 0;
    size_t offset;
    size_t first_local_bag;
    size_t generator_index;
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

        offset = next;
    }

    if(inst_start == 0 || ibag_start == 0 || igen_start == 0 || inst_size < 2 * SF2_INSTRUMENT_SIZE ||
       ibag_size < 3 * SF2_BAG_SIZE || igen_size < SF2_GENERATOR_SIZE)
    {
        goto cleanup;
    }

    /* Instrument zero has a global bag followed by its sample-bearing bag. */
    first_local_bag = read_u16(data + inst_start + SF2_INSTRUMENT_SIZE + 20) - 1;
    if(first_local_bag + 1 >= ibag_size / SF2_BAG_SIZE)
    {
        goto cleanup;
    }

    generator_index = read_u16(data + ibag_start + first_local_bag * SF2_BAG_SIZE);
    if(generator_index >= igen_size / SF2_GENERATOR_SIZE)
    {
        goto cleanup;
    }

    write_u16(data + igen_start + generator_index * SF2_GENERATOR_SIZE, generator_id);
    write_u16(data + igen_start + generator_index * SF2_GENERATOR_SIZE + 2, 200);

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
    static const char *const sfont_paths[] = {
        "sfont-keynum-bounds.sf2",
        "sfont-rootkey-bounds.sf2"
    };
    static const uint16_t fixed_key_generators[] = {
        SF2_KEYNUM_GENERATOR,
        SF2_OVERRIDEROOTKEY_GENERATOR
    };
    fluid_settings_t *settings;
    fluid_synth_t *synth;
    double pitches[128];
    int i, test;

    for(test = 0; test < 2; ++test)
    {
        const char *sfont_path = sfont_paths[test];
        TEST_ASSERT(create_sfont_with_invalid_fixed_key(sfont_path, fixed_key_generators[test]));
        settings = new_fluid_settings();
        TEST_ASSERT(settings != NULL);
        synth = new_fluid_synth(settings);
        TEST_ASSERT(synth != NULL);
        TEST_ASSERT(fluid_synth_sfload(synth, sfont_path, 1) != FLUID_FAILED);
        TEST_SUCCESS(fluid_synth_program_change(synth, 0, 127));

        for(i = 0; i < 128; ++i)
        {
            pitches[i] = i * 100.0;
        }
        TEST_SUCCESS(fluid_synth_activate_key_tuning(synth, 0, 1, "fixed key bounds", pitches, 0));
        TEST_SUCCESS(fluid_synth_activate_tuning(synth, 0, 0, 1, 1));
        TEST_SUCCESS(fluid_synth_noteon(synth, 0, 60, 100));
        TEST_ASSERT(voice_has_actual_key(synth, 60, 60));

        delete_fluid_synth(synth);
        delete_fluid_settings(settings);
        TEST_ASSERT(remove(sfont_path) == 0);
    }

    settings = new_fluid_settings();
    TEST_ASSERT(settings != NULL);
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);
    TEST_ASSERT(fluid_synth_sfload(synth, TEST_SOUNDFONT, 1) != FLUID_FAILED);
    TEST_SUCCESS(fluid_synth_program_change(synth, 0, 127));

    for(i = 0; i < 128; ++i)
    {
        pitches[i] = i * 100.0;
    }
    TEST_SUCCESS(fluid_synth_activate_key_tuning(synth, 0, 1, "API key bounds", pitches, 0));
    TEST_SUCCESS(fluid_synth_activate_tuning(synth, 0, 0, 1, 1));
    TEST_SUCCESS(fluid_synth_set_gen(synth, 0, SF2_KEYNUM_GENERATOR, 200.0f));
    TEST_SUCCESS(fluid_synth_noteon(synth, 0, 60, 100));
    TEST_ASSERT(voice_has_actual_key(synth, 60, 127));

    delete_fluid_synth(synth);
    delete_fluid_settings(settings);

    return EXIT_SUCCESS;
}
