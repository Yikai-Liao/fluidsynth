#include "test.h"
#include "fluidsynth.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SF2_SAMPLE_HEADER_SIZE 46u

static uint32_t read_u32(const unsigned char *data)
{
    return (uint32_t)data[0] | (uint32_t)data[1] << 8 | (uint32_t)data[2] << 16 | (uint32_t)data[3] << 24;
}

static void write_u32(unsigned char *data, uint32_t value)
{
    data[0] = (unsigned char)value;
    data[1] = (unsigned char)(value >> 8);
    data[2] = (unsigned char)(value >> 16);
    data[3] = (unsigned char)(value >> 24);
}

static int create_sfont_with_sample_end_past_data(const char *path)
{
    FILE *input = fopen(TEST_SOUNDFONT, "rb");
    FILE *output = NULL;
    unsigned char *data = NULL;
    long file_length;
    size_t file_size;
    size_t offset;
    size_t sample_headers = 0, sample_headers_size = 0;
    size_t sample_count = 0;
    size_t i;
    int found_sample = 0;
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
        const size_t chunk_end = offset + 8 + chunk_size;
        const size_t next = chunk_end + (chunk_size & 1u);

        if(next > file_size)
        {
            goto cleanup;
        }

        if(memcmp(data + offset, "LIST", 4) == 0 && chunk_size >= 4)
        {
            const unsigned char *list_type = data + offset + 8;
            size_t child = offset + 12;

            while(child + 8 <= chunk_end)
            {
                const uint32_t child_size = read_u32(data + child + 4);
                const size_t child_end = child + 8 + child_size;
                const size_t child_next = child_end + (child_size & 1u);

                if(child_next > chunk_end)
                {
                    goto cleanup;
                }

                if(memcmp(list_type, "sdta", 4) == 0 && memcmp(data + child, "smpl", 4) == 0)
                {
                    sample_count = child_size / sizeof(short);
                }
                else if(memcmp(list_type, "pdta", 4) == 0 && memcmp(data + child, "shdr", 4) == 0)
                {
                    sample_headers = child + 8;
                    sample_headers_size = child_size;
                }

                child = child_next;
            }
        }

        offset = next;
    }

    if(sample_count == 0 || sample_headers == 0 || sample_headers_size % SF2_SAMPLE_HEADER_SIZE != 0)
    {
        goto cleanup;
    }

    /* Fm3_a1 is used by preset 0 for MIDI key 60 in the shipped test font. */
    for(i = 0; i + 1 < sample_headers_size / SF2_SAMPLE_HEADER_SIZE; ++i)
    {
        unsigned char *header = data + sample_headers + i * SF2_SAMPLE_HEADER_SIZE;

        if(memcmp(header, "Fm3_a1", sizeof("Fm3_a1") - 1) == 0)
        {
            /* SF2 end is exclusive. This makes the imported inclusive end one past the data. */
            write_u32(header + 24, (uint32_t)sample_count + 1u);
            found_sample = 1;
            break;
        }
    }

    if(!found_sample)
    {
        goto cleanup;
    }

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
    const char *const sfont_path = "sfont-sample-end-bounds.sf2";
    fluid_settings_t *settings;
    fluid_synth_t *synth;
    int sfont_id;

    TEST_ASSERT(create_sfont_with_sample_end_past_data(sfont_path));
    settings = new_fluid_settings();
    TEST_ASSERT(settings != NULL);
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);

    sfont_id = fluid_synth_sfload(synth, sfont_path, 1);
    if(sfont_id != FLUID_FAILED)
    {
        TEST_SUCCESS(fluid_synth_program_change(synth, 0, 0));
        TEST_SUCCESS(fluid_synth_noteon(synth, 0, 60, 100));
        TEST_ASSERT(fluid_synth_get_active_voice_count(synth) == 0);
    }

    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    TEST_ASSERT(remove(sfont_path) == 0);
    return EXIT_SUCCESS;
}
