#include "test.h"
#include "fluidsynth.h"
#include "fluid_synth.h"

#include <math.h>

static float render_peak(fluid_synth_t *synth, int blocks)
{
    float left[FLUID_BUFSIZE];
    float right[FLUID_BUFSIZE];
    float peak = 0.0f;
    int b, i;

    for(b = 0; b < blocks; b++)
    {
        TEST_SUCCESS(fluid_synth_write_float(synth, FLUID_BUFSIZE, left, 0, 1, right, 0, 1));
        for(i = 0; i < FLUID_BUFSIZE; i++)
        {
            float l = fabsf(left[i]);
            float r = fabsf(right[i]);
            TEST_ASSERT(isfinite(left[i]) && isfinite(right[i]));
            if(l > peak)
            {
                peak = l;
            }
            if(r > peak)
            {
                peak = r;
            }
        }
    }
    return peak;
}

/* Verify all seven synth.limiter.* settings update at runtime without recreating the synth. */
int main(void)
{
    fluid_settings_t *settings = new_fluid_settings();
    fluid_synth_t *synth;
    fluid_sfont_t *sfont;
    int font, voices;
    size_t i;
    float limited, tighter;
    const char *names[] =
    {
        "synth.limiter.active",
        "synth.limiter.output-limit",
        "synth.limiter.attack",
        "synth.limiter.hold",
        "synth.limiter.release",
        "synth.limiter.smoothing-stages",
        "synth.limiter.link-channels"
    };

    TEST_ASSERT(settings != NULL);
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.reverb.active", 0));
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.chorus.active", 0));
    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.gain", 5.0));

    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);

    for(i = 0; i < FLUID_N_ELEMENTS(names); i++)
    {
        TEST_ASSERT(fluid_settings_is_realtime(settings, names[i]));
    }

    font = fluid_synth_sfload(synth, TEST_SOUNDFONT, 1);
    TEST_ASSERT(font >= 0);
    sfont = fluid_synth_get_sfont_by_id(synth, font);
    TEST_SUCCESS(fluid_synth_program_select(synth, 0, font, 0, 5));
    TEST_SUCCESS(fluid_synth_noteon(synth, 0, 60, 127));
    voices = fluid_synth_get_active_voice_count(synth);
    TEST_ASSERT(voices > 0);

    /* Inactive limiter passes loud output above the later limit threshold. */
    TEST_ASSERT(render_peak(synth, 20) > 0.5f);

    /* Enabling and updating parameters limits output without reloading SoundFonts or voices. */
    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.output-limit", 0.25));
    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.attack", 5.0));
    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.hold", 10.0));
    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.release", 20.0));
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.limiter.smoothing-stages", 2));
    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.link-channels", 1.0));
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.limiter.active", 1));
    render_peak(synth, 30);
    limited = render_peak(synth, 30);
    TEST_ASSERT(limited > 0.05f && limited <= 0.26f);

    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.output-limit", 0.1));
    render_peak(synth, 30);
    tighter = render_peak(synth, 30);
    TEST_ASSERT(tighter > 0.02f && tighter <= 0.11f);

    /* Increasing attack resets buffered audio to the new look-ahead delay. */
    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.attack", 250.0));
    TEST_ASSERT(render_peak(synth, 1) == 0.0f);

    /* Disabling returns to un-limited bypass immediately. */
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.limiter.active", 0));
    TEST_ASSERT(render_peak(synth, 20) > 0.3f);

    TEST_ASSERT(fluid_synth_get_sfont_by_id(synth, font) == sfont);
    TEST_ASSERT(fluid_synth_get_active_voice_count(synth) == voices);

    TEST_SUCCESS(fluid_synth_all_notes_off(synth, 0));
    delete_fluid_synth(synth);

    /* Settings outlive the synth and can configure a new synth after callbacks unregister. */
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.limiter.active", 1));
    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.limiter.attack", 5.0));
    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);

    return EXIT_SUCCESS;
}
