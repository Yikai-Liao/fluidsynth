#include "test.h"
#include "fluidsynth.h"
#include "fluidsynth/mod.h"
#include "fluidsynth/sfont.h"
#include <stdint.h>
#include "fluid_voice.h"
#include <limits.h>
#include <stdint.h>
#include <string.h>

#define SF_MOD_SIZE 10
#define DMOD_CHUNK_SIZE (8 + 2 * SF_MOD_SIZE)

static uint32_t read_u32(const unsigned char *data)
{
    return (uint32_t)data[0] | (uint32_t)data[1] << 8 | (uint32_t)data[2] << 16 | (uint32_t)data[3] << 24;
}

static void write_u16(unsigned char *data, uint16_t value)
{
    data[0] = (unsigned char)value;
    data[1] = (unsigned char)(value >> 8);
}

static void write_u32(unsigned char *data, uint32_t value)
{
    data[0] = (unsigned char)value;
    data[1] = (unsigned char)(value >> 8);
    data[2] = (unsigned char)(value >> 16);
    data[3] = (unsigned char)(value >> 24);
}

/* Add a DMOD to the shipped SF2 so the test exercises imported default mods. */
static int create_sfont_with_dmod(const char *path, uint16_t destination)
{
    FILE *input = fopen(TEST_SOUNDFONT, "rb");
    FILE *output;
    unsigned char *data;
    long file_length;
    size_t file_size, info_size, insert_at;
    int result = 0;

    if (input == NULL || fseek(input, 0, SEEK_END) != 0 || (file_length = ftell(input)) < 0 ||
        fseek(input, 0, SEEK_SET) != 0)
    {
        if (input != NULL)
        {
            fclose(input);
        }

        return 0;
    }

    file_size = (size_t)file_length;
    if (file_size < 24)
    {
        fclose(input);
        return 0;
    }

    data = malloc(file_size + DMOD_CHUNK_SIZE);
    if (data == NULL || fread(data, 1, file_size, input) != file_size)
    {
        free(data);
        fclose(input);
        return 0;
    }
    fclose(input);

    /* TEST_SOUNDFONT begins with LIST[INFO], where the optional DMOD belongs. */
    info_size = read_u32(data + 16);
    insert_at = 20 + info_size;
    if (memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "sfbk", 4) != 0 ||
        memcmp(data + 12, "LIST", 4) != 0 || memcmp(data + 20, "INFO", 4) != 0 || info_size < 4 ||
        insert_at > file_size)
    {
        goto cleanup;
    }

    memmove(data + insert_at + DMOD_CHUNK_SIZE, data + insert_at, file_size - insert_at);
    memset(data + insert_at, 0, DMOD_CHUNK_SIZE);
    memcpy(data + insert_at, "DMOD", 4);
    write_u32(data + insert_at + 4, 2 * SF_MOD_SIZE);
    write_u16(data + insert_at + 8, FLUID_MOD_VELOCITY);
    write_u16(data + insert_at + 10, destination);
    write_u16(data + insert_at + 12, 1);
    write_u32(data + 16, (uint32_t)(info_size + DMOD_CHUNK_SIZE));
    write_u32(data + 4, (uint32_t)(file_size + DMOD_CHUNK_SIZE - 8));

    output = fopen(path, "wb");
    if (output != NULL)
    {
        result = fwrite(data, 1, file_size + DMOD_CHUNK_SIZE, output) == file_size + DMOD_CHUNK_SIZE;
        if (fclose(output) != 0)
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
    static const char *const bad_sfont_path = "sfont-mod-dest-bounds.sf2";
    fluid_settings_t *settings;
    fluid_synth_t *synth;
    fluid_sfont_t *sfont;
    fluid_mod_t *mod;
    int sfid;

    TEST_ASSERT(create_sfont_with_dmod(bad_sfont_path, UINT16_MAX));

    settings = new_fluid_settings();
    TEST_ASSERT(settings != NULL);
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);
    sfid = fluid_synth_sfload(synth, bad_sfont_path, 1);
    TEST_ASSERT(sfid != FLUID_FAILED);
    sfont = fluid_synth_get_sfont_by_id(synth, sfid);
    TEST_ASSERT(sfont != NULL);
    TEST_SUCCESS(fluid_synth_program_change(synth, 0, 127));
    TEST_SUCCESS(fluid_synth_noteon(synth, 0, 60, 100));

    mod = new_fluid_mod();
    TEST_ASSERT(mod != NULL);
    fluid_mod_set_source1(mod, FLUID_MOD_VELOCITY,
                          FLUID_MOD_GC | FLUID_MOD_POSITIVE | FLUID_MOD_UNIPOLAR | FLUID_MOD_LINEAR);
    fluid_mod_set_dest(mod, 65535);
    TEST_ASSERT(fluid_mod_get_dest(mod) == UCHAR_MAX);
    TEST_ASSERT(fluid_synth_add_default_mod(synth, mod, FLUID_SYNTH_ADD) == FLUID_FAILED);
    TEST_SUCCESS(fluid_sfont_set_default_mod(sfont, NULL, 1));
    TEST_ASSERT(fluid_sfont_set_default_mod(sfont, mod, 1) == FLUID_FAILED);
    fluid_mod_set_dest(mod, 256);
    TEST_ASSERT(fluid_mod_get_dest(mod) == UCHAR_MAX);
    TEST_ASSERT(fluid_synth_add_default_mod(synth, mod, FLUID_SYNTH_ADD) == FLUID_FAILED);
    TEST_ASSERT(fluid_sfont_set_default_mod(sfont, mod, 1) == FLUID_FAILED);

    delete_fluid_mod(mod);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);

    TEST_ASSERT(create_sfont_with_dmod(bad_sfont_path, 48));
    settings = new_fluid_settings();
    TEST_ASSERT(settings != NULL);
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);
    sfid = fluid_synth_sfload(synth, bad_sfont_path, 1);
    TEST_ASSERT(sfid != FLUID_FAILED);
    sfont = fluid_synth_get_sfont_by_id(synth, sfid);
    TEST_ASSERT(sfont != NULL);
    TEST_SUCCESS(fluid_synth_program_change(synth, 0, 127));

    mod = new_fluid_mod();
    TEST_ASSERT(mod != NULL);
    fluid_mod_set_source1(mod, FLUID_MOD_VELOCITY,
                          FLUID_MOD_GC | FLUID_MOD_POSITIVE | FLUID_MOD_UNIPOLAR | FLUID_MOD_LINEAR);
    fluid_mod_set_dest(mod, 48);
    fluid_mod_set_amount(mod, 1.0);
    TEST_SUCCESS(fluid_sfont_set_default_mod(sfont, mod, 1));
    TEST_SUCCESS(fluid_synth_noteon(synth, 0, 60, 100));
    /* NULL clears the list regardless of the nonnegative count, as documented. */
    TEST_SUCCESS(fluid_sfont_set_default_mod(sfont, NULL, 1));
    TEST_SUCCESS(fluid_synth_noteon(synth, 0, 61, 100));
    TEST_SUCCESS(fluid_sfont_set_default_mod(sfont, NULL, 0));

    {
        fluid_voice_t *voices[512];
        fluid_voice_t *voice;
        int mod_count;

        fluid_synth_get_voicelist(synth, voices, 512, -1);
        TEST_ASSERT(voices[0] != NULL);
        voice = voices[0];
        mod_count = voice->mod_count;

        /* A custom preset may add a mod directly to a live voice. Keep this
         * public path from storing an invalid generator destination. */
        fluid_mod_set_source1(mod, 1,
                              FLUID_MOD_CC | FLUID_MOD_POSITIVE |
                              FLUID_MOD_UNIPOLAR | FLUID_MOD_LINEAR);
        fluid_mod_set_dest(mod, UCHAR_MAX);
        fluid_voice_add_mod(voice, mod, FLUID_VOICE_ADD);
        TEST_SUCCESS(fluid_synth_cc(synth, 0, 1, 127));
        TEST_ASSERT(voice->mod_count == mod_count);
    }

    delete_fluid_mod(mod);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    remove(bad_sfont_path);
    return EXIT_SUCCESS;
}
