#include "test.h"
#include "fluidsynth.h"
#include <stdint.h>
#include "fluid_voice.h"

#ifdef FLUID_TEST_WRAP_SEQ_SEND
static int fail_noteoff;
static int rejected_noteoffs;

int __real_fluid_sequencer_send_at(fluid_sequencer_t *seq, fluid_event_t *evt,
                                 unsigned int time, int absolute);

int __wrap_fluid_sequencer_send_at(fluid_sequencer_t *seq, fluid_event_t *evt,
                                 unsigned int time, int absolute)
{
    if(fail_noteoff && fluid_event_get_type(evt) == FLUID_SEQ_NOTEOFF)
    {
        rejected_noteoffs++;
        return FLUID_FAILED;
    }

    return __real_fluid_sequencer_send_at(seq, evt, time, absolute);
}
#endif

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
    int sfont_id, seqid;

    TEST_ASSERT(settings != NULL);
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);
    sfont_id = fluid_synth_sfload(synth, TEST_SOUNDFONT, 1);
    TEST_ASSERT(sfont_id != FLUID_FAILED);
    TEST_SUCCESS(fluid_synth_program_select(synth, 1, sfont_id, 0, 0));

    seq = new_fluid_sequencer2(0);
    TEST_ASSERT(seq != NULL);
    seqid = fluid_sequencer_register_fluidsynth(seq, synth);
    TEST_ASSERT(seqid != FLUID_FAILED);
    evt = new_fluid_event();
    TEST_ASSERT(evt != NULL);

    fluid_event_note(evt, 1, 60, 100, 1000);
    send_event(seq, evt, seqid, 10);
    TEST_ASSERT(latest_note_has_noteoff(synth, 1, 60) == 0);

    fluid_event_all_notes_off(evt, 0);
    send_event(seq, evt, seqid, 20);

    fluid_event_note(evt, 1, 60, 100, 2000);
    send_event(seq, evt, seqid, 30);
    TEST_ASSERT(latest_note_has_noteoff(synth, 1, 60) == 0);

    // The channel-0 ALL_NOTES_OFF must not erase channel 1's overlap tracking.
    // Otherwise this old note-off is delivered to the newer channel-1 note.
    fluid_sequencer_process(seq, 1010);
    TEST_ASSERT(latest_note_has_noteoff(synth, 1, 60) == 0);

    fluid_event_note(evt, 1, 61, 100, 1000);
    send_event(seq, evt, seqid, 1020);

    fluid_event_all_sounds_off(evt, 0);
    send_event(seq, evt, seqid, 1030);

    fluid_event_note(evt, 1, 61, 100, 2000);
    send_event(seq, evt, seqid, 1040);
    fluid_sequencer_process(seq, 2020);
    TEST_ASSERT(latest_note_has_noteoff(synth, 1, 61) == 0);

    fluid_sequencer_process(seq, 2030);
    fluid_event_note(evt, 1, 60, 100, 1000);
    send_event(seq, evt, seqid, 2040);

    fluid_event_all_notes_off(evt, 1);
    send_event(seq, evt, seqid, 2050);

    fluid_event_note(evt, 1, 60, 100, 2000);
    send_event(seq, evt, seqid, 2060);
    fluid_sequencer_process(seq, 3040);
    TEST_ASSERT(latest_note_has_noteoff(synth, 1, 60) == 0);

    fluid_event_all_notes_off(evt, 1);
    send_event(seq, evt, seqid, 3050);
    fluid_event_noteon(evt, 1, 60, 100);
    send_event(seq, evt, seqid, 3060);
    fluid_sequencer_process(seq, 4060);
    TEST_ASSERT(latest_note_has_noteoff(synth, 1, 60) == 0);

    /* A failed replacement must leave the original duration intact. */
    fluid_event_note(evt, 1, 62, 100, 1000);
    send_event(seq, evt, seqid, 4070);
    TEST_ASSERT(latest_note_has_noteoff(synth, 1, 62) == 0);
    TEST_SUCCESS(fluid_synth_unset_program(synth, 1));
    fluid_event_note(evt, 1, 62, 100, 2000);
    send_event(seq, evt, seqid, 4080);
    fluid_sequencer_process(seq, 5070);
    TEST_ASSERT(latest_note_has_noteoff(synth, 1, 62) == 1);

#ifdef FLUID_TEST_WRAP_SEQ_SEND
    TEST_SUCCESS(fluid_synth_program_select(synth, 1, sfont_id, 0, 0));
    fluid_event_note(evt, 1, 63, 100, 1000);
    send_event(seq, evt, seqid, 5080);
    TEST_ASSERT(latest_note_has_noteoff(synth, 1, 63) == 0);

    /* If the new deadline cannot be queued, the new voice must be released. */
    fail_noteoff = 1;
    fluid_event_note(evt, 1, 63, 100, 2000);
    send_event(seq, evt, seqid, 5090);
    fail_noteoff = 0;
    TEST_ASSERT(rejected_noteoffs == 1);
    TEST_ASSERT(latest_note_has_noteoff(synth, 1, 63) == 1);
#endif

    fluid_sequencer_unregister_client(seq, seqid);
    delete_fluid_event(evt);
    delete_fluid_sequencer(seq);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    return EXIT_SUCCESS;
}
