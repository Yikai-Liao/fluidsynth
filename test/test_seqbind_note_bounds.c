#include "test.h"
#include "fluidsynth.h"

#include <stdint.h>
#include <limits.h>

#include "fluid_voice.h"

static int latest_note_has_noteoff(fluid_synth_t *synth, int channel, int key)
{
    fluid_voice_t *voices[512];
    unsigned int latest_id = 0;
    int result = -1;
    int i;

    fluid_synth_get_voicelist(synth, voices, 512, -1);
    for(i = 0; i < 512 && voices[i] != NULL; ++i)
    {
        fluid_voice_t *voice = voices[i];
        if(voice->chan == channel && voice->key == key && voice->id >= latest_id)
        {
            latest_id = voice->id;
            result = voice->has_noteoff;
        }
    }

    return result;
}

static void send_event(fluid_sequencer_t *seq, fluid_event_t *evt,
                       int dest, unsigned int time)
{
    fluid_event_set_source(evt, -1);
    fluid_event_set_dest(evt, dest);
    TEST_SUCCESS(fluid_sequencer_send_at(seq, evt, time, 1));
    fluid_sequencer_process(seq, time);
}

int main(void)
{
    fluid_settings_t *settings = new_fluid_settings();
    fluid_synth_t *synth;
    fluid_sequencer_t *seq;
    fluid_event_t *evt;
    int sfont_id;
    int seqid;
    const short invalid_velocities[] = {-1, 128, SHRT_MAX};
    int i;

    TEST_ASSERT(settings != NULL);
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);
    sfont_id = fluid_synth_sfload(synth, TEST_SOUNDFONT, 1);
    TEST_ASSERT(sfont_id != FLUID_FAILED);
    TEST_SUCCESS(fluid_synth_program_select(synth, 0, sfont_id, 0, 0));

    seq = new_fluid_sequencer2(0);
    TEST_ASSERT(seq != NULL);
    seqid = fluid_sequencer_register_fluidsynth(seq, synth);
    TEST_ASSERT(seqid != FLUID_FAILED);
    evt = new_fluid_event();
    TEST_ASSERT(evt != NULL);

    fluid_event_note(evt, 0, 60, 100, 1000);
    send_event(seq, evt, seqid, 10);
    TEST_ASSERT(latest_note_has_noteoff(synth, 0, 60) == 0);

    /* Invalid note fields must not collide with a valid channel/key ID. */
    fluid_event_note(evt, 1, -68, 100, 2000);
    send_event(seq, evt, seqid, 20);

    fluid_sequencer_process(seq, 1010);
    TEST_ASSERT(latest_note_has_noteoff(synth, 0, 60) == 1);

    fluid_event_note(evt, 0, 61, 100, 1000);
    send_event(seq, evt, seqid, 1020);

    /* 128 * 33,554,432 wraps to zero and used to collide with channel 0/key 61. */
    fluid_event_note(evt, 33554432, 61, 100, 2000);
    send_event(seq, evt, seqid, 1030);

    fluid_sequencer_process(seq, 2020);
    TEST_ASSERT(latest_note_has_noteoff(synth, 0, 61) == 1);

    for(i = 0; i < 3; i++)
    {
        unsigned int time = 2030 + i * 1020;
        short key = 62 + i;

        fluid_event_note(evt, 0, key, 100, 1000);
        send_event(seq, evt, seqid, time);
        TEST_ASSERT(latest_note_has_noteoff(synth, 0, key) == 0);

        /* Rejected velocity must not replace the existing note's deadline. */
        fluid_event_note(evt, 0, key, invalid_velocities[i], 2000);
        send_event(seq, evt, seqid, time + 10);
        fluid_sequencer_process(seq, time + 1000);
        TEST_ASSERT(latest_note_has_noteoff(synth, 0, key) == 1);
    }

    /* Preserve the binding's existing zero-velocity note-off behavior. */
    fluid_event_note(evt, 0, 65, 100, 1000);
    send_event(seq, evt, seqid, 5080);
    TEST_ASSERT(latest_note_has_noteoff(synth, 0, 65) == 0);
    fluid_event_note(evt, 0, 65, 0, 2000);
    send_event(seq, evt, seqid, 5090);
    TEST_ASSERT(latest_note_has_noteoff(synth, 0, 65) == 1);

    fluid_sequencer_unregister_client(seq, seqid);
    delete_fluid_event(evt);
    delete_fluid_sequencer(seq);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    return EXIT_SUCCESS;
}
