#define _POSIX_C_SOURCE 200809L
#include <fluidsynth.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double stamp(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void checked(int status)
{
    if(status == FLUID_FAILED)
    {
        fprintf(stderr, "FluidSynth API failure\n");
        exit(2);
    }
}

static int compare_double(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

int main(int argc, char **argv)
{
    if(argc != 5)
    {
        fprintf(stderr, "Usage: render_latency_probe SF2 MIDI seconds latency_csv\n");
        return 2;
    }

    const int block = 64;
    const int sample_rate = 44100;
    const size_t frames = (size_t)(atof(argv[3]) * sample_rate);
    const size_t blocks = (frames + block - 1) / block;
    float *audio = calloc(2 * frames, sizeof(float));
    double *latency = calloc(blocks, sizeof(double));
    if(!audio || !latency)
    {
        return 2;
    }

    fluid_settings_t *settings = new_fluid_settings();
    checked(fluid_settings_setnum(settings, "synth.sample-rate", sample_rate));
    checked(fluid_settings_setnum(settings, "synth.gain", 0.2));
    checked(fluid_settings_setint(settings, "synth.polyphony", 256));
    checked(fluid_settings_setint(settings, "synth.cpu-cores", 1));
    checked(fluid_settings_setint(settings, "synth.reverb.active", 0));
    checked(fluid_settings_setint(settings, "synth.chorus.active", 0));
    checked(fluid_settings_setint(settings, "audio.realtime-prio", 0));
    checked(fluid_settings_setint(settings, "synth.lock-memory", 0));
    checked(fluid_settings_setstr(settings, "player.timing-source", "sample"));

    fluid_synth_t *synth = new_fluid_synth(settings);
    if(!synth || fluid_synth_sfload(synth, argv[1], 1) == FLUID_FAILED)
    {
        return 2;
    }
    checked(fluid_synth_set_interp_method(synth, -1, FLUID_INTERP_4THORDER));
    fluid_player_t *player = new_fluid_player(synth);
    if(!player)
    {
        return 2;
    }
    checked(fluid_player_add(player, argv[2]));
    checked(fluid_player_play(player));

    for(size_t b = 0; b < blocks; b++)
    {
        size_t first = b * block;
        int n = (int)((frames - first < block) ? frames - first : block);
        double start = stamp();
        checked(fluid_synth_write_float(synth, n, audio + 2 * first, 0, 2,
                                        audio + 2 * first, 1, 2));
        latency[b] = stamp() - start;
    }

    FILE *csv = fopen(argv[4], "w");
    if(!csv)
    {
        return 2;
    }
    fprintf(csv, "block,latency_us\n");
    for(size_t b = 0; b < blocks; b++)
    {
        fprintf(csv, "%zu,%.6f\n", b, latency[b] * 1e6);
    }
    fclose(csv);

    double max_latency = 0.0;
    size_t max_block = 0;
    for(size_t b = 0; b < blocks; b++)
    {
        if(latency[b] > max_latency)
        {
            max_latency = latency[b];
            max_block = b;
        }
    }
    qsort(latency, blocks, sizeof(double), compare_double);
    size_t p99 = (size_t)(0.99 * (blocks - 1));
    size_t p999 = (size_t)(0.999 * (blocks - 1));

    uint64_t hash = UINT64_C(14695981039346656037);
    const unsigned char *bytes = (const unsigned char *)audio;
    for(size_t i = 0; i < 2 * frames * sizeof(float); i++)
    {
        hash = (hash ^ bytes[i]) * UINT64_C(1099511628211);
    }
    printf("{\"frames\":%zu,\"blocks\":%zu,\"p50_us\":%.6f,\"p99_us\":%.6f,\"p999_us\":%.6f,\"max_us\":%.6f,\"max_block\":%zu,\"hash\":\"%016llx\"}\n",
           frames, blocks, latency[blocks / 2] * 1e6, latency[p99] * 1e6,
           latency[p999] * 1e6, max_latency * 1e6, max_block,
           (unsigned long long)hash);

    checked(fluid_player_stop(player));
    delete_fluid_player(player);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    free(latency);
    free(audio);
    return 0;
}
