#include "test.h"
#include "fluidsynth.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

#define SF2_INSTRUMENT_SIZE 22u
#define SF2_BAG_SIZE 4u
#define SF2_GENERATOR_SIZE 4u
#define SF2_OVERRIDEROOTKEY_GENERATOR 58u
#define SF2_VELOCITY_GENERATOR 47u

static uint16_t read_u16(const unsigned char *data)
{
    return (uint16_t)data[0] | (uint16_t)data[1] << 8;
}

static uint32_t read_u32(const unsigned char *data)
{
    return (uint32_t)data[0] | (uint32_t)data[1] << 8 | (uint32_t)data[2] << 16 | (uint32_t)data[3] << 24;
}

static void write_u16(unsigned char *data, uint16_t value)
{
    data[0] = (unsigned char)value;
    data[1] = (unsigned char)(value >> 8);
}

static int create_sfont_with_zero_fixed_velocity(const char *path)
{
    FILE *input = fopen(TEST_SOUNDFONT, "rb");
    FILE *output = NULL;
    unsigned char *data = NULL;
    long file_length;
    size_t file_size, offset, pdta_start = 0, pdta_end = 0;
    size_t inst_start = 0, inst_size = 0, ibag_start = 0, ibag_size = 0;
    size_t igen_start = 0, igen_size = 0, first_local_bag, generator_index;
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

    if(inst_start == 0 || ibag_start == 0 || igen_start == 0 ||
       inst_size < 2 * SF2_INSTRUMENT_SIZE || ibag_size < 3 * SF2_BAG_SIZE ||
       igen_size < SF2_GENERATOR_SIZE)
    {
        goto cleanup;
    }

    /* Replace the first zone's root override with the legal fixed-velocity
     * generator; the sample already has root key 60. */
    first_local_bag = read_u16(data + inst_start + SF2_INSTRUMENT_SIZE + 20) - 1;
    if(first_local_bag + 1 >= ibag_size / SF2_BAG_SIZE)
    {
        goto cleanup;
    }
    generator_index = read_u16(data + ibag_start + first_local_bag * SF2_BAG_SIZE);
    if(generator_index >= igen_size / SF2_GENERATOR_SIZE ||
       read_u16(data + igen_start + generator_index * SF2_GENERATOR_SIZE) != SF2_OVERRIDEROOTKEY_GENERATOR)
    {
        goto cleanup;
    }
    write_u16(data + igen_start + generator_index * SF2_GENERATOR_SIZE, SF2_VELOCITY_GENERATOR);
    write_u16(data + igen_start + generator_index * SF2_GENERATOR_SIZE + 2, 0);

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
    static const char *const sfont_path = "sfont-fixed-velocity-zero.sf2";
    fluid_settings_t *settings;
    fluid_synth_t *synth;
    float audio[1024];
    float peak = 0.0f;
    fluid_voice_t *voices[512];
    fluid_mod_t *mod;
    int sfid, i;

    TEST_ASSERT(create_sfont_with_zero_fixed_velocity(sfont_path));
    settings = new_fluid_settings();
    TEST_ASSERT(settings != NULL);
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);
    sfid = fluid_synth_sfload(synth, sfont_path, 1);
    TEST_ASSERT(sfid != FLUID_FAILED);
    TEST_SUCCESS(fluid_synth_program_select(synth, 0, sfid, 0, 127));
    TEST_SUCCESS(fluid_synth_noteon(synth, 0, 60, 100));
    fluid_synth_get_voicelist(synth, voices, 512, -1);
    TEST_ASSERT(voices[0] != NULL);
    TEST_ASSERT(fluid_voice_get_actual_velocity(voices[0]) == 0);
    TEST_SUCCESS(fluid_synth_write_float(synth, 512, audio, 0, 2, audio, 1, 2));

    for(i = 0; i < 1024; ++i)
    {
        peak = fmaxf(peak, fabsf(audio[i]));
    }
    TEST_ASSERT(peak < 0.0001f);

    /* Fixed velocity changes modulator input, rather than acting as NOTE OFF.
     * Disable velocity-to-attenuation and the same zero-velocity zone can sound. */
    TEST_SUCCESS(fluid_synth_all_sounds_off(synth, 0));
    mod = new_fluid_mod();
    TEST_ASSERT(mod != NULL);
    fluid_mod_set_source1(mod, FLUID_MOD_VELOCITY,
                         FLUID_MOD_GC | FLUID_MOD_NEGATIVE |
                         FLUID_MOD_UNIPOLAR | FLUID_MOD_CONCAVE);
    fluid_mod_set_source2(mod, 0, 0);
    fluid_mod_set_dest(mod, GEN_ATTENUATION);
    fluid_mod_set_amount(mod, 0.0);
    TEST_SUCCESS(fluid_synth_add_default_mod(synth, mod, FLUID_SYNTH_OVERWRITE));
    delete_fluid_mod(mod);
    TEST_SUCCESS(fluid_synth_noteon(synth, 0, 60, 100));
    fluid_synth_get_voicelist(synth, voices, 512, -1);
    TEST_ASSERT(voices[0] != NULL);
    TEST_ASSERT(fluid_voice_get_actual_velocity(voices[0]) == 0);
    TEST_SUCCESS(fluid_synth_write_float(synth, 512, audio, 0, 2, audio, 1, 2));
    peak = 0.0f;
    for(i = 0; i < 1024; ++i)
    {
        peak = fmaxf(peak, fabsf(audio[i]));
    }
    TEST_ASSERT(peak > 0.0001f);

    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    TEST_ASSERT(remove(sfont_path) == 0);
    return EXIT_SUCCESS;
}
