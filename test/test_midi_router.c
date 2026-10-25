#include "test.h"
#include "midi/fluid_midi.h"
#include <string.h>

typedef struct
{
    int count;
    int fail_at;
    fluid_midi_event_t events[16];
} captured_events_t;

static int capture_event(void *data, fluid_midi_event_t *event)
{
    captured_events_t *captured = data;
    TEST_ASSERT((size_t)captured->count < FLUID_N_ELEMENTS(captured->events));
    captured->events[captured->count] = *event;
    captured->count++;
    return captured->count == captured->fail_at ? FLUID_FAILED : FLUID_OK;
}

static fluid_midi_router_t *new_test_router_with_channels(captured_events_t *captured,
        int midi_channels)
{
    fluid_settings_t *settings = new_fluid_settings();
    fluid_midi_router_t *router;
    TEST_ASSERT(settings != NULL);
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.midi-channels", midi_channels));
    router = new_fluid_midi_router(settings, capture_event, captured);
    delete_fluid_settings(settings);
    TEST_ASSERT(router != NULL);
    TEST_SUCCESS(fluid_midi_router_clear_rules(router));
    return router;
}

static fluid_midi_router_t *new_test_router(captured_events_t *captured)
{
    return new_test_router_with_channels(captured, 16);
}

static fluid_midi_event_t *make_event(int type, int channel, int par1, int par2)
{
    fluid_midi_event_t *event = new_fluid_midi_event();
    TEST_ASSERT(event != NULL);
    TEST_SUCCESS(fluid_midi_event_set_type(event, type));
    TEST_SUCCESS(fluid_midi_event_set_channel(event, channel));
    event->param1 = (unsigned int)par1;
    event->param2 = (unsigned int)par2;
    return event;
}

static void add_rule(fluid_midi_router_t *router, int type,
                     int chan_min, int chan_max, float chan_mul, int chan_add,
                     int par1_min, int par1_max, float par1_mul, int par1_add,
                     int par2_min, int par2_max, float par2_mul, int par2_add)
{
    fluid_midi_router_rule_t *rule = new_fluid_midi_router_rule();
    TEST_ASSERT(rule != NULL);
    fluid_midi_router_rule_set_chan(rule, chan_min, chan_max, chan_mul, chan_add);
    fluid_midi_router_rule_set_param1(rule, par1_min, par1_max, par1_mul, par1_add);
    fluid_midi_router_rule_set_param2(rule, par2_min, par2_max, par2_mul, par2_add);
    TEST_SUCCESS(fluid_midi_router_add_rule(router, rule, type));
}

static void test_note_conversion_and_ranges(void)
{
    captured_events_t captured = {0};
    fluid_midi_router_t *router = new_test_router(&captured);
    fluid_midi_event_t *event;

    /* Inverted windows include both endpoints and exclude only interior values. */
    add_rule(router, FLUID_MIDI_ROUTER_RULE_NOTE, 5, 2, 0.5f, 0,
             70, 60, 1.0f, 0, 80, 20, 1.0f, 0);

    event = make_event(NOTE_ON, 2, 70, 10);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    TEST_ASSERT(captured.count == 1);
    TEST_ASSERT(captured.events[0].type == NOTE_ON);
    TEST_ASSERT(captured.events[0].channel == 1); /* 2 * .5 maps to channel 1 */
    TEST_ASSERT(captured.events[0].param1 == 70 && captured.events[0].param2 == 10);
    delete_fluid_midi_event(event);

    event = make_event(NOTE_ON, 5, 60, 20); /* inverted window endpoints are inclusive */
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    TEST_ASSERT(captured.count == 2);
    TEST_ASSERT(captured.events[1].channel == 3);
    TEST_ASSERT(captured.events[1].param1 == 60 && captured.events[1].param2 == 20);
    delete_fluid_midi_event(event);

    event = make_event(NOTE_ON, 3, 70, 10); /* excluded by inverted channel window */
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    TEST_ASSERT(captured.count == 2);
    delete_fluid_midi_event(event);

    event = make_event(NOTE_ON, 2, 65, 10); /* excluded by inverted parameter window */
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    TEST_ASSERT(captured.count == 2);
    delete_fluid_midi_event(event);

    /* A zero-velocity note-on is normalized to note-off before rule matching. */
    event = make_event(NOTE_ON, 2, 70, 0);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    TEST_ASSERT(captured.count == 3);
    TEST_ASSERT(captured.events[2].type == NOTE_OFF);
    TEST_ASSERT(captured.events[2].param2 == 127);
    delete_fluid_midi_event(event);

    delete_fluid_midi_router(router);
}

static void test_clamping_and_invalid_values(void)
{
    captured_events_t captured = {0};
    fluid_midi_router_t *router = new_test_router(&captured);
    fluid_midi_event_t *event;

    add_rule(router, FLUID_MIDI_ROUTER_RULE_NOTE, 0, 15, 1.0f, 0,
             0, 127, 1.0f, 100, 0, 127, 1.0f, 100);
    event = make_event(NOTE_ON, 0, 60, 60);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    TEST_ASSERT(captured.count == 1);
    TEST_ASSERT(captured.events[0].param1 == 127 && captured.events[0].param2 == 127);
    delete_fluid_midi_event(event);

    add_rule(router, FLUID_MIDI_ROUTER_RULE_CC, 0, 15, 1.0f, 0,
             0, 127, 1.0f, 127, 0, 127, 1.0f, 100);
    event = make_event(CONTROL_CHANGE, 0, 1, 1);
    TEST_ASSERT(fluid_midi_router_handle_midi_event(router, event) == FLUID_FAILED);
    TEST_ASSERT(captured.count == 1); /* invalid controller numbers are dropped */
    delete_fluid_midi_event(event);

    add_rule(router, FLUID_MIDI_ROUTER_RULE_PROG_CHANGE, 0, 15, 1.0f, 16,
             0, 127, 1.0f, 0, 0, 127, 1.0f, 0);
    event = make_event(PROGRAM_CHANGE, 0, 3, 0);
    TEST_ASSERT(fluid_midi_router_handle_midi_event(router, event) == FLUID_FAILED);
    TEST_ASSERT(captured.count == 1); /* invalid output channels are dropped */
    delete_fluid_midi_event(event);

    add_rule(router, FLUID_MIDI_ROUTER_RULE_PROG_CHANGE, 0, 15, 1.0f, 0,
             0, 127, 1.0f, 0, 0, 127, 1.0f, 0);
    event = make_event(PROGRAM_CHANGE, 0, 3, 0);
    TEST_ASSERT(fluid_midi_router_handle_midi_event(router, event) == FLUID_FAILED);
    TEST_ASSERT(captured.count == 2 && captured.events[1].param1 == 3);
    delete_fluid_midi_event(event);

    add_rule(router, FLUID_MIDI_ROUTER_RULE_PITCH_BEND, 0, 15, 1.0f, 0,
             0, 16383, 1.0f, 1000, 0, 127, 1.0f, 0);
    event = make_event(PITCH_BEND, 1, 16000, 0);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    TEST_ASSERT(captured.count == 3);
    TEST_ASSERT(captured.events[2].param1 == 16383);
    delete_fluid_midi_event(event);

    delete_fluid_midi_router(router);
}

static void test_pressure_and_system_messages(void)
{
    captured_events_t captured = {0};
    fluid_midi_router_t *router = new_test_router(&captured);
    fluid_midi_event_t *event;
    char sysex[] = {0x7e, 0x7f, 0x09, 0x01};

    add_rule(router, FLUID_MIDI_ROUTER_RULE_CHANNEL_PRESSURE, 0, 15, 1.0f, 0,
             0, 127, 1.0f, 1000, 0, 127, 1.0f, 0);
    event = make_event(CHANNEL_PRESSURE, 0, 8, 0);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    TEST_ASSERT(captured.count == 1 && captured.events[0].param1 == 127);
    delete_fluid_midi_event(event);

    add_rule(router, FLUID_MIDI_ROUTER_RULE_KEY_PRESSURE, 0, 15, 1.0f, 0,
             0, 127, 1.0f, -100, 0, 127, 1.0f, -100);
    event = make_event(KEY_PRESSURE, 1, 8, 8);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    TEST_ASSERT(captured.count == 2);
    TEST_ASSERT(captured.events[1].type == KEY_PRESSURE);
    TEST_ASSERT(captured.events[1].param1 == 0 && captured.events[1].param2 == 0);
    delete_fluid_midi_event(event);

    event = make_event(MIDI_SYSTEM_RESET, 0, 0, 0);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    TEST_ASSERT(captured.count == 3 && captured.events[2].type == MIDI_SYSTEM_RESET);
    delete_fluid_midi_event(event);

    event = make_event(MIDI_SYSEX, 0, 0, 0);
    TEST_SUCCESS(fluid_midi_event_set_sysex(event, sysex, sizeof(sysex), FALSE));
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    TEST_ASSERT(captured.count == 4 && captured.events[3].type == MIDI_SYSEX);
    TEST_ASSERT(captured.events[3].paramptr == sysex);
    TEST_ASSERT(captured.events[3].param1 == sizeof(sysex));
    delete_fluid_midi_event(event);

    event = make_event(MIDI_TIME_CODE, 0, 0, 0);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    TEST_ASSERT(captured.count == 4); /* unsupported system messages are ignored */
    delete_fluid_midi_event(event);

    delete_fluid_midi_router(router);
}

static void test_pending_note_off_when_rules_are_cleared(void)
{
    captured_events_t captured = {0};
    fluid_midi_router_t *router = new_test_router(&captured);
    fluid_midi_event_t *event;

    add_rule(router, FLUID_MIDI_ROUTER_RULE_NOTE, 0, 15, 1.0f, 0,
             0, 127, 1.0f, 0, 0, 127, 1.0f, 0);
    event = make_event(NOTE_ON, 4, 64, 90);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    TEST_ASSERT(captured.count == 1);
    delete_fluid_midi_event(event);

    TEST_SUCCESS(fluid_midi_router_clear_rules(router));
    event = make_event(NOTE_ON, 4, 65, 90);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    TEST_ASSERT(captured.count == 1); /* waiting rules suppress new note-ons */
    delete_fluid_midi_event(event);

    event = make_event(NOTE_OFF, 4, 65, 45);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    TEST_ASSERT(captured.count == 1); /* its note-on was never forwarded */
    delete_fluid_midi_event(event);

    event = make_event(NOTE_OFF, 4, 64, 45);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    TEST_ASSERT(captured.count == 2); /* final note-off removes the waiting rule */
    TEST_ASSERT(captured.events[1].type == NOTE_OFF && captured.events[1].param1 == 64);
    delete_fluid_midi_event(event);

    event = make_event(NOTE_ON, 4, 66, 90);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    TEST_ASSERT(captured.count == 2);
    delete_fluid_midi_event(event);

    event = make_event(NOTE_OFF, 4, 66, 45);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    TEST_ASSERT(captured.count == 2); /* no stale waiting rule forwards a ghost note-off */
    delete_fluid_midi_event(event);

    delete_fluid_midi_router(router);
}

static void test_default_rules_preserve_pending_route(void)
{
    captured_events_t captured = {0};
    fluid_midi_router_t *router = new_test_router(&captured);
    fluid_midi_event_t *event;

    add_rule(router, FLUID_MIDI_ROUTER_RULE_NOTE, 0, 15, 1.0f, 2,
             0, 127, 1.0f, 10, 0, 127, 1.0f, 0);
    event = make_event(NOTE_ON, 0, 60, 90);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    delete_fluid_midi_event(event);
    TEST_ASSERT(captured.count == 1 && captured.events[0].channel == 2 &&
                captured.events[0].param1 == 70);

    TEST_SUCCESS(fluid_midi_router_set_default_rules(router));
    event = make_event(NOTE_ON, 0, 60, 90);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    delete_fluid_midi_event(event);
    TEST_ASSERT(captured.count == 2 && captured.events[1].channel == 0 &&
                captured.events[1].param1 == 60);

    event = make_event(NOTE_OFF, 0, 60, 45);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    delete_fluid_midi_event(event);
    /* The same input pitch now has a pending note on both output routes. */
    TEST_ASSERT(captured.count == 4 && captured.events[2].channel == 0 &&
                captured.events[2].param1 == 60 && captured.events[3].channel == 2 &&
                captured.events[3].param1 == 70);

    delete_fluid_midi_router(router);
}

static void test_router_channel_and_parameter_window_boundaries(void)
{
    captured_events_t captured = {0};
    fluid_midi_router_t *router = new_test_router(&captured);
    fluid_midi_event_t *event;

    add_rule(router, FLUID_MIDI_ROUTER_RULE_CC, 4, 6, 1.0f, 0,
             30, 40, 1.0f, 0, 50, 60, 1.0f, 0);

    event = make_event(CONTROL_CHANGE, 4, 30, 50);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    delete_fluid_midi_event(event);
    event = make_event(CONTROL_CHANGE, 6, 40, 60);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    delete_fluid_midi_event(event);
    TEST_ASSERT(captured.count == 2); /* normal windows include both endpoints */

    event = make_event(CONTROL_CHANGE, 3, 30, 50);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    delete_fluid_midi_event(event);
    event = make_event(CONTROL_CHANGE, 7, 30, 50);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    delete_fluid_midi_event(event);
    event = make_event(CONTROL_CHANGE, 5, 29, 50);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    delete_fluid_midi_event(event);
    event = make_event(CONTROL_CHANGE, 5, 41, 50);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    delete_fluid_midi_event(event);
    event = make_event(CONTROL_CHANGE, 5, 30, 49);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    delete_fluid_midi_event(event);
    event = make_event(CONTROL_CHANGE, 5, 30, 61);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    delete_fluid_midi_event(event);
    TEST_ASSERT(captured.count == 2);

    TEST_SUCCESS(fluid_midi_router_clear_rules(router));
    add_rule(router, FLUID_MIDI_ROUTER_RULE_CC, 5, 5, 1.0f, 0,
             10, 10, 1.0f, 0, 50, 50, 1.0f, 0);

    event = make_event(CONTROL_CHANGE, 5, 10, 50);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    delete_fluid_midi_event(event);
    TEST_ASSERT(captured.count == 3); /* equal min/max windows match only that value */

    event = make_event(CONTROL_CHANGE, 4, 10, 50);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    delete_fluid_midi_event(event);
    event = make_event(CONTROL_CHANGE, 6, 10, 50);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    delete_fluid_midi_event(event);
    event = make_event(CONTROL_CHANGE, 5, 9, 50);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    delete_fluid_midi_event(event);
    event = make_event(CONTROL_CHANGE, 5, 11, 50);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    delete_fluid_midi_event(event);
    event = make_event(CONTROL_CHANGE, 5, 10, 49);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    delete_fluid_midi_event(event);
    event = make_event(CONTROL_CHANGE, 5, 10, 51);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    delete_fluid_midi_event(event);
    TEST_ASSERT(captured.count == 3);

    delete_fluid_midi_router(router);
}

static void test_router_state_at_max_channel(void)
{
    captured_events_t captured = {0};
    fluid_midi_router_t *router = new_test_router_with_channels(&captured, 256);
    fluid_midi_event_t *event;

    TEST_SUCCESS(fluid_midi_router_clear_rules(router));
    add_rule(router, FLUID_MIDI_ROUTER_RULE_NOTE, 0, 255, 1.0f, 0,
             0, 127, 1.0f, 0, 0, 127, 1.0f, 0);
    add_rule(router, FLUID_MIDI_ROUTER_RULE_CC, 0, 255, 1.0f, 0,
             0, 127, 1.0f, 0, 0, 127, 1.0f, 0);

    event = make_event(NOTE_ON, 255, 127, 90);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    delete_fluid_midi_event(event);
    event = make_event(CONTROL_CHANGE, 255, 64, 127);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    delete_fluid_midi_event(event);
    TEST_SUCCESS(fluid_midi_router_clear_rules(router));

    event = make_event(NOTE_OFF, 255, 127, 45);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    delete_fluid_midi_event(event);
    event = make_event(CONTROL_CHANGE, 255, 64, 0);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    delete_fluid_midi_event(event);

    TEST_ASSERT(captured.count == 4);
    TEST_ASSERT(captured.events[0].channel == 255 && captured.events[1].channel == 255);
    TEST_ASSERT(captured.events[2].channel == 255 && captured.events[3].channel == 255);
    delete_fluid_midi_router(router);
}

static void test_cc_endpoints_and_sustain_threshold(void)
{
    captured_events_t captured = {0};
    fluid_midi_router_t *router = new_test_router(&captured);
    fluid_midi_event_t *event;

    add_rule(router, FLUID_MIDI_ROUTER_RULE_CC, 0, 15, 1.0f, 0,
             0, 127, 1.0f, 0, 0, 127, 1.0f, 0);

    event = make_event(CONTROL_CHANGE, 0, 0, 0);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    delete_fluid_midi_event(event);
    event = make_event(CONTROL_CHANGE, 0, 127, 127);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    delete_fluid_midi_event(event);
    TEST_ASSERT(captured.count == 2); /* controller and value endpoints are valid */

    event = make_event(CONTROL_CHANGE, 0, 64, 64);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    delete_fluid_midi_event(event);
    TEST_ASSERT(captured.count == 3);

    TEST_SUCCESS(fluid_midi_router_clear_rules(router));
    event = make_event(CONTROL_CHANGE, 0, 64, 63);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    delete_fluid_midi_event(event);
    TEST_ASSERT(captured.count == 4); /* sustain becomes pending at value 64 */

    event = make_event(CONTROL_CHANGE, 0, 64, 64);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    delete_fluid_midi_event(event);
    TEST_ASSERT(captured.count == 4); /* the cleared rule was removed on pedal-up */

    delete_fluid_midi_router(router);
}

static void test_unmatched_note_off_does_not_keep_rule_pending(void)
{
    captured_events_t captured = {0};
    fluid_midi_router_t *router = new_test_router(&captured);
    fluid_midi_event_t *event;

    add_rule(router, FLUID_MIDI_ROUTER_RULE_NOTE, 0, 15, 1.0f, 0,
             0, 127, 1.0f, 0, 0, 127, 1.0f, 0);
    event = make_event(NOTE_OFF, 2, 60, 45);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    delete_fluid_midi_event(event);
    TEST_ASSERT(captured.count == 1);

    TEST_SUCCESS(fluid_midi_router_clear_rules(router));
    add_rule(router, FLUID_MIDI_ROUTER_RULE_NOTE, 0, 15, 1.0f, 0,
             0, 127, 1.0f, 0, 0, 127, 1.0f, 0);
    event = make_event(NOTE_ON, 2, 60, 90);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    delete_fluid_midi_event(event);
    event = make_event(NOTE_OFF, 2, 60, 45);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    delete_fluid_midi_event(event);

    TEST_ASSERT(captured.count == 3); /* the new rule forwards exactly one note-off */
    delete_fluid_midi_router(router);
}

static void test_pending_same_note_on_separate_channels(void)
{
    captured_events_t captured = {0};
    fluid_midi_router_t *router = new_test_router(&captured);
    fluid_midi_event_t *event;

    add_rule(router, FLUID_MIDI_ROUTER_RULE_NOTE, 0, 15, 1.0f, 0,
             0, 127, 1.0f, 0, 0, 127, 1.0f, 0);

    event = make_event(NOTE_ON, 4, 64, 90);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    delete_fluid_midi_event(event);

    event = make_event(NOTE_ON, 5, 64, 90);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    delete_fluid_midi_event(event);
    TEST_ASSERT(captured.count == 2);

    TEST_SUCCESS(fluid_midi_router_clear_rules(router));

    event = make_event(NOTE_OFF, 4, 64, 45);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    delete_fluid_midi_event(event);

    event = make_event(NOTE_OFF, 5, 64, 45);
    TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
    delete_fluid_midi_event(event);

    /* Clearing rules must keep routing note-offs for every active channel. */
    TEST_ASSERT(captured.count == 4);
    TEST_ASSERT(captured.events[2].channel == 4 && captured.events[3].channel == 5);
    delete_fluid_midi_router(router);
}

static void test_note_off_ignores_velocity_window_and_callback_failure(void)
{
    captured_events_t captured = {0};
    fluid_midi_router_t *router = new_test_router(&captured);
    fluid_midi_event_t *event;

    add_rule(router, FLUID_MIDI_ROUTER_RULE_NOTE, 0, 15, 1.0f, 0,
             0, 127, 1.0f, 0, 100, 127, 1.0f, 0);
    event = make_event(NOTE_OFF, 0, 60, 1); /* velocity windows do not filter note-offs */
    captured.fail_at = 1;
    TEST_ASSERT(fluid_midi_router_handle_midi_event(router, event) == FLUID_FAILED);
    TEST_ASSERT(captured.count == 1 && captured.events[0].type == NOTE_OFF);
    delete_fluid_midi_event(event);

    delete_fluid_midi_router(router);
}

int main(void)
{
    test_note_conversion_and_ranges();
    test_clamping_and_invalid_values();
    test_pressure_and_system_messages();
    test_pending_note_off_when_rules_are_cleared();
    test_default_rules_preserve_pending_route();
    test_pending_same_note_on_separate_channels();
    test_router_channel_and_parameter_window_boundaries();
    test_router_state_at_max_channel();
    test_cc_endpoints_and_sustain_threshold();
    test_unmatched_note_off_does_not_keep_rule_pending();
    test_note_off_ignores_velocity_window_and_callback_failure();
    return EXIT_SUCCESS;
}
