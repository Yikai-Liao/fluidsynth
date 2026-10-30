#include "test.h"
#include "fluidsynth.h"
#include "fluid_midi.h"

/* Count forwarded pedal events without requiring a synthesizer. */
static int count_event(void *data, fluid_midi_event_t *event)
{
    int *count = data;
    TEST_ASSERT(fluid_midi_event_get_type(event) == CONTROL_CHANGE);
    (*count)++;
    return FLUID_OK;
}

int main(void)
{
    fluid_settings_t *settings = new_fluid_settings();
    fluid_midi_event_t *event = new_fluid_midi_event();
    int pedal, count = 0;
    fluid_midi_router_t *router = new_fluid_midi_router(settings, count_event, &count);
    TEST_ASSERT(settings != NULL && event != NULL && router != NULL);
    TEST_SUCCESS(fluid_midi_event_set_type(event, CONTROL_CHANGE));
    TEST_SUCCESS(fluid_midi_event_set_channel(event, 0));

    for(pedal = SUSTAIN_SWITCH; pedal <= SOSTENUTO_SWITCH; pedal += 2)
    {
        count = 0;
        TEST_SUCCESS(fluid_midi_router_set_default_rules(router));
        TEST_SUCCESS(fluid_midi_event_set_control(event, pedal));
        TEST_SUCCESS(fluid_midi_event_set_value(event, 127));
        TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
        TEST_ASSERT(count == 1);
        TEST_SUCCESS(fluid_midi_router_clear_rules(router));
        TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
        TEST_ASSERT(count == 1);
        TEST_SUCCESS(fluid_midi_event_set_value(event, 0));
        TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
        TEST_ASSERT(count == 2);
        TEST_SUCCESS(fluid_midi_router_handle_midi_event(router, event));
        TEST_ASSERT(count == 2);
    }

    delete_fluid_midi_router(router);
    delete_fluid_midi_event(event);
    delete_fluid_settings(settings);
    return EXIT_SUCCESS;
}
