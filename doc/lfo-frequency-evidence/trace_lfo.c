#include "fluidsynth.h"
#include "fluid_sys.h"
#include "fluid_voice.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REQUIRE(condition) do { if(!(condition)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); exit(2); } } while(0)

int main(int argc, char **argv)
{
    fluid_settings_t *settings;
    fluid_synth_t *synth;
    fluid_voice_t *voices[2] = {0};
    fluid_voice_t *voice;
    fluid_lfo_t *lfo;
    FILE *csv, *pcm;
    char path[4096];
    float left[FLUID_BUFSIZE], right[FLUID_BUFSIZE];
    int sparse, fixed_depth, cc = 20;
    unsigned int n, blocks, next_event = 0;
    const unsigned int sparse_blocks[] = {750, 1125, 1875, 3375};
    const int sparse_values[] = {127, 20, 127, 20};

    if(argc < 4 || argc > 5)
    {
        fprintf(stderr, "Usage: %s SF2 dense|sparse OUTPUT_PREFIX [--fixed-depth]\n", argv[0]);
        return 2;
    }
    REQUIRE(!strcmp(argv[2], "dense") || !strcmp(argv[2], "sparse"));
    sparse = !strcmp(argv[2], "sparse");
    fixed_depth = argc == 5;
    REQUIRE(!fixed_depth || !strcmp(argv[4], "--fixed-depth"));
    settings = new_fluid_settings();
    REQUIRE(settings != NULL);
    REQUIRE(fluid_settings_setnum(settings, "synth.sample-rate", 48000) == FLUID_OK);
    REQUIRE(fluid_settings_setnum(settings, "synth.gain", 0.5) == FLUID_OK);
    REQUIRE(fluid_settings_setint(settings, "synth.cpu-cores", 1) == FLUID_OK);
    REQUIRE(fluid_settings_setint(settings, "synth.polyphony", 8) == FLUID_OK);
    REQUIRE(fluid_settings_setint(settings, "synth.reverb.active", 0) == FLUID_OK);
    REQUIRE(fluid_settings_setint(settings, "synth.chorus.active", 0) == FLUID_OK);
    synth = new_fluid_synth(settings);
    REQUIRE(synth != NULL);
    if(fixed_depth)
    {
        fluid_mod_t *mod = new_fluid_mod();
        REQUIRE(mod != NULL);
        /* Keep the SF2 frequency mapping and remove only the default depth mapping. */
        fluid_mod_set_source1(mod, 1, FLUID_MOD_CC | FLUID_MOD_LINEAR);
        fluid_mod_set_source2(mod, 0, 0);
        fluid_mod_set_dest(mod, GEN_VIBLFOTOPITCH);
        fluid_mod_set_amount(mod, 50);
        REQUIRE(fluid_synth_remove_default_mod(synth, mod) == FLUID_OK);
        delete_fluid_mod(mod);
    }
    REQUIRE(fluid_synth_sfload(synth, argv[1], 1) >= 0);
    REQUIRE(fluid_synth_cc(synth, 0, 1, cc) == FLUID_OK);
    REQUIRE(fluid_synth_noteon(synth, 0, 60, 126) == FLUID_OK);
    fluid_synth_get_voicelist(synth, voices, FLUID_N_ELEMENTS(voices), -1);
    REQUIRE(voices[0] != NULL && voices[1] == NULL);
    voice = voices[0];
    lfo = &voice->rvoice->envlfo.viblfo;
    snprintf(path, sizeof(path), "%s.csv", argv[3]);
    csv = fopen(path, "w");
    REQUIRE(csv != NULL);
    snprintf(path, sizeof(path), "%s.f32", argv[3]);
    pcm = fopen(path, "wb");
    REQUIRE(pcm != NULL);
    fprintf(csv, "time,cc,event,val_before,val,increment_before,increment,depth_before,depth,phase_incr,ticks,delay\n");

    blocks = sparse ? 4500 : 4605;
    for(n = 0; n < blocks; n++)
    {
        int event = 0;
        fluid_real_t val_before = lfo->val;
        fluid_real_t increment_before = lfo->increment;
        fluid_real_t depth_before = voice->rvoice->envlfo.viblfo_to_pitch;
        if(sparse && next_event < FLUID_N_ELEMENTS(sparse_blocks) && n == sparse_blocks[next_event])
        {
            cc = sparse_values[next_event++];
            event = 1;
        }
        else if(!sparse && next_event < 107 && n == 1500 + 15 * next_event)
        {
            cc = 21 + next_event++;
            event = 1;
        }
        if(event)
        {
            REQUIRE(fluid_synth_cc(synth, 0, 1, cc) == FLUID_OK);
        }
        REQUIRE(fluid_synth_write_float(synth, FLUID_BUFSIZE, left, 0, 1, right, 0, 1) == FLUID_OK);
        REQUIRE(voice->rvoice->envlfo.ticks == (unsigned int)((n + 1) * FLUID_BUFSIZE));
        REQUIRE(fluid_synth_get_active_voice_count(synth) == 1);
        fprintf(csv, "%.12g,%d,%d,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%u,%u\n",
                n * FLUID_BUFSIZE / 48000.0, cc, event, (double)val_before, (double)lfo->val,
                (double)increment_before, (double)lfo->increment, (double)depth_before,
                (double)voice->rvoice->envlfo.viblfo_to_pitch,
                (double)voice->rvoice->dsp.phase_incr, voice->rvoice->envlfo.ticks, lfo->delay);
        REQUIRE(fwrite(left, sizeof(float), FLUID_BUFSIZE, pcm) == FLUID_BUFSIZE);
    }
    REQUIRE(next_event == (sparse ? FLUID_N_ELEMENTS(sparse_blocks) : 107));
    fclose(csv);
    fclose(pcm);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    return EXIT_SUCCESS;
}
