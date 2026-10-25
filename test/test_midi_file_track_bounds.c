#include "test.h"
#include "fluidsynth.h"
#include "midi/fluid_midi.h"

static int rejects_malformed_file(const unsigned char *midi, size_t size)
{
    float left[2048], right[2048];
    fluid_settings_t *settings = new_fluid_settings();
    fluid_synth_t *synth = NULL;
    fluid_player_t *player = NULL;
    int rejected = 0;

    if(settings == NULL)
    {
        goto cleanup;
    }

    synth = new_fluid_synth(settings);
    if(synth == NULL)
    {
        goto cleanup;
    }

    player = new_fluid_player(synth);
    if(player == NULL || fluid_player_add_mem(player, midi, size) != FLUID_OK ||
       fluid_player_play(player) != FLUID_OK ||
       fluid_synth_write_float(synth, 2048, left, 0, 1, right, 0, 1) != FLUID_OK)
    {
        goto cleanup;
    }

    rejected = player->ntracks == 0;

cleanup:
    if(player != NULL)
    {
        delete_fluid_player(player);
    }

    if(synth != NULL)
    {
        delete_fluid_synth(synth);
    }

    delete_fluid_settings(settings);
    return rejected;
}

static int accepts_channel_and_meta_events(const unsigned char *midi, size_t size)
{
    float left[128], right[128];
    fluid_settings_t *settings = new_fluid_settings();
    fluid_synth_t *synth = NULL;
    fluid_player_t *player = NULL;
    fluid_midi_event_t *event;
    int event_count = 0;

    if(settings == NULL)
    {
        goto cleanup;
    }

    synth = new_fluid_synth(settings);
    if(synth == NULL)
    {
        goto cleanup;
    }

    player = new_fluid_player(synth);
    if(player == NULL || fluid_player_add_mem(player, midi, size) != FLUID_OK ||
       fluid_player_play(player) != FLUID_OK ||
       fluid_synth_write_float(synth, 128, left, 0, 1, right, 0, 1) != FLUID_OK ||
       player->ntracks != 1 ||
       player->track[0] == NULL || player->track[0]->name == NULL ||
       strcmp(player->track[0]->name, "trk") != 0)
    {
        goto cleanup;
    }

    for(event = player->track[0]->first; event != NULL; event = event->next)
    {
        event_count++;
    }

cleanup:
    if(player != NULL)
    {
        delete_fluid_player(player);
    }

    if(synth != NULL)
    {
        delete_fluid_synth(synth);
    }

    delete_fluid_settings(settings);
    return event_count == 11;
}

static int accepts_long_text_event(void)
{
    static const unsigned char header[] = {
        'M','T','h','d', 0,0,0,6, 0,0, 0,1, 0,96,
        'M','T','r','k', 0,0,1,8
    };
    unsigned char midi[286];
    float left[2], right[2];
    fluid_settings_t *settings = new_fluid_settings();
    fluid_synth_t *synth = NULL;
    fluid_player_t *player = NULL;
    fluid_midi_event_t *event;
    void *text = NULL;
    int text_size = 0;
    size_t pos = 0;
    int accepted = 0;

    if(settings == NULL)
    {
        goto cleanup;
    }

    memcpy(midi, header, sizeof(header));
    pos = sizeof(header);
    midi[pos++] = 0;
    midi[pos++] = 0xff;
    midi[pos++] = 0x01;
    midi[pos++] = 0x81;
    midi[pos++] = 0x7f;
    memset(midi + pos, 'x', 255);
    pos += 255;
    midi[pos++] = 0;
    midi[pos++] = 0xff;
    midi[pos++] = 0x2f;
    midi[pos++] = 0;

    if(pos != sizeof(midi))
    {
        goto cleanup;
    }

    synth = new_fluid_synth(settings);
    if(synth == NULL)
    {
        goto cleanup;
    }

    player = new_fluid_player(synth);
    if(player == NULL || fluid_player_add_mem(player, midi, sizeof(midi)) != FLUID_OK ||
       fluid_player_play(player) != FLUID_OK ||
       fluid_synth_write_float(synth, 2, left, 0, 1, right, 0, 1) != FLUID_OK ||
       player->ntracks != 1 || player->track[0] == NULL)
    {
        goto cleanup;
    }

    event = player->track[0]->first;
    if(event == NULL || fluid_midi_event_get_type(event) != MIDI_TEXT ||
       fluid_midi_event_get_text(event, &text, &text_size) != FLUID_OK ||
       text == NULL || text_size != 256 || ((char *)text)[0] != 'x' ||
       ((char *)text)[254] != 'x' || ((char *)text)[255] != '\0' ||
       event->next == NULL || fluid_midi_event_get_type(event->next) != MIDI_EOT ||
       event->next->next != NULL)
    {
        goto cleanup;
    }

    accepted = 1;

cleanup:
    if(player != NULL)
    {
        delete_fluid_player(player);
    }

    if(synth != NULL)
    {
        delete_fluid_synth(synth);
    }

    delete_fluid_settings(settings);
    return accepted;
}

int main(void)
{
    static const unsigned char channel_and_meta_events[] = {
        'M','T','h','d', 0,0,0,6, 0,0, 0,1, 0,96,
        'M','T','r','k', 0,0,0,76,
        0,0x90,60,100, 0,0x80,60,0, 0,0xa0,60,50,
        0,0xb0,7,100, 0,0xc0,5, 0,0xd0,64, 0,0xe0,0,64,
        0,0xf0,3,0x7e,0x7f,0xf7,
        0,0xff,0x01,3,'a','b','c', 0,0xff,0x03,3,'t','r','k',
        0,0xff,0x51,3,7,0xa1,0x20, 0,0xff,0x58,4,4,2,24,8,
        0,0xff,0x59,2,0,0, 0,0xff,0x7f,1,0, 0,0xff,0x2f,0
    };
    static const unsigned char channel_event_crosses_track[] = {
        'M','T','h','d', 0,0,0,6, 0,0, 0,1, 0,96,
        'M','T','r','k', 0,0,0,1,
        0, 0x90,60,100, 0,0xff,0x2f,0
    };
    static const unsigned char meta_payload_crosses_track[] = {
        'M','T','h','d', 0,0,0,6, 0,0, 0,1, 0,96,
        'M','T','r','k', 0,0,0,4,
        0,0xff,0x01,3, 'a','b','c'
    };
    static const unsigned char running_status_after_meta[] = {
        'M','T','h','d', 0,0,0,6, 0,0, 0,1, 0,96,
        'M','T','r','k', 0,0,0,15,
        0,0x90,60,100, 0,0xff,0x06,0, 0,61,100, 0,0xff,0x2f,0
    };
    static const unsigned char large_unknown_chunk_length[] = {
        'M','T','h','d', 0,0,0,6, 0,0, 0,1, 0,96,
        'J','U','N','K', 0x80,0,0,0,
        'M','T','r','k', 0,0,0,4, 0,0xff,0x2f,0
    };
    static const unsigned char large_track_length_without_payload[] = {
        'M','T','h','d', 0,0,0,6, 0,0, 0,1, 0,96,
        'M','T','r','k', 0x80,0,0,0
    };

    TEST_ASSERT(rejects_malformed_file(channel_event_crosses_track,
                                        sizeof(channel_event_crosses_track)));
    TEST_ASSERT(rejects_malformed_file(meta_payload_crosses_track,
                                       sizeof(meta_payload_crosses_track)));
    TEST_ASSERT(rejects_malformed_file(running_status_after_meta,
                                       sizeof(running_status_after_meta)));
    TEST_ASSERT(rejects_malformed_file(large_unknown_chunk_length,
                                       sizeof(large_unknown_chunk_length)));
    TEST_ASSERT(rejects_malformed_file(large_track_length_without_payload,
                                       sizeof(large_track_length_without_payload)));
    TEST_ASSERT(accepts_channel_and_meta_events(channel_and_meta_events,
                                                 sizeof(channel_and_meta_events)));
    TEST_ASSERT(accepts_long_text_event());
    return EXIT_SUCCESS;
}
