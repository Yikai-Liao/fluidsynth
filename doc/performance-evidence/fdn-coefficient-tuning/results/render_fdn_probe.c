#define _POSIX_C_SOURCE 200809L
#include <fluidsynth.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/resource.h>

static double stamp(clockid_t clock) {
    struct timespec ts;
    clock_gettime(clock, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}
static void checked(int status) {
    if (status == FLUID_FAILED) { fprintf(stderr, "FluidSynth API failure\n"); exit(2); }
}
int main(int argc, char **argv) {
    if (argc < 7) {
        fprintf(stderr, "Usage: render_probe SF2 MIDI block cores seconds fx [pcm_output [sample_rate [room_size [damp [width]]]]]\n");
        return 2;
    }
    int block = atoi(argv[3]), cores = atoi(argv[4]), fx = atoi(argv[6]);
    int sample_rate = argc > 8 ? atoi(argv[8]) : 44100;
    double room_size = argc > 9 ? atof(argv[9]) : 1.0;
    double damp = argc > 10 ? atof(argv[10]) : 0.0;
    double width = argc > 11 ? atof(argv[11]) : 100.0;
    size_t frames = (size_t)(atof(argv[5]) * sample_rate);
    if (block <= 0 || !frames) return 2;
    double setup_start = stamp(CLOCK_MONOTONIC);
    fluid_settings_t *settings = new_fluid_settings();
    if (!settings) return 2;
    checked(fluid_settings_setnum(settings, "synth.sample-rate", sample_rate));
    checked(fluid_settings_setnum(settings, "synth.gain", 0.2));
    checked(fluid_settings_setint(settings, "synth.polyphony", 256));
    checked(fluid_settings_setint(settings, "synth.cpu-cores", cores));
    checked(fluid_settings_setint(settings, "audio.realtime-prio", 0));
    checked(fluid_settings_setint(settings, "synth.lock-memory", 0));
    checked(fluid_settings_setint(settings, "synth.reverb.active", fx));
    checked(fluid_settings_setstr(settings, "synth.reverb.engine", "fdn"));
    checked(fluid_settings_setnum(settings, "synth.reverb.room-size", room_size));
    checked(fluid_settings_setnum(settings, "synth.reverb.damp", damp));
    checked(fluid_settings_setnum(settings, "synth.reverb.width", width));
    checked(fluid_settings_setnum(settings, "synth.reverb.level", 0.7));
    checked(fluid_settings_setint(settings, "synth.chorus.active", 0));
    checked(fluid_settings_setstr(settings, "player.timing-source", "sample"));
    fluid_synth_t *synth = new_fluid_synth(settings);
    if (!synth || fluid_synth_sfload(synth, argv[1], 1) == FLUID_FAILED) return 2;
    checked(fluid_synth_set_interp_method(synth, -1, FLUID_INTERP_4THORDER));
    fluid_player_t *player = new_fluid_player(synth);
    if (!player) return 2;
    checked(fluid_player_add(player, argv[2]));
    float *audio = calloc(2 * frames, sizeof(float));
    if (!audio) return 2;
    /* Touch pages before the rendering timer. */
    volatile float *touch = audio;
    for (size_t i = 0; i < 2 * frames; i += 1024) touch[i] = 0;
    double setup_wall = stamp(CLOCK_MONOTONIC) - setup_start;
    checked(fluid_player_play(player));
    double cpu_start = stamp(CLOCK_PROCESS_CPUTIME_ID);
    double wall_start = stamp(CLOCK_MONOTONIC);
    for (size_t i = 0; i < frames; i += block) {
        int n = frames - i < (size_t)block ? frames - i : (size_t)block;
        checked(fluid_synth_write_float(synth, n, audio + 2*i, 0, 2, audio + 2*i, 1, 2));
    }
    double render_wall = stamp(CLOCK_MONOTONIC) - wall_start;
    double render_cpu = stamp(CLOCK_PROCESS_CPUTIME_ID) - cpu_start;
    uint64_t hash = UINT64_C(14695981039346656037);
    double energy = 0, peak = 0;
    const unsigned char *bytes = (const unsigned char *)audio;
    for (size_t i = 0; i < frames * 2 * sizeof(float); i++) {
        hash = (hash ^ bytes[i]) * UINT64_C(1099511628211);
    }
    for (size_t i = 0; i < frames * 2; i++) {
        if (!isfinite(audio[i])) return 3;
        energy += (double)audio[i]*audio[i];
        if (fabs(audio[i]) > peak) peak = fabs(audio[i]);
    }
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    if (argc > 7) {
        FILE *out = fopen(argv[7], "wb");
        if (!out || fwrite(audio, sizeof(float)*2, frames, out) != frames) return 2;
        fclose(out);
    }
    printf("{\"block\":%d,\"cores\":%d,\"fx\":%d,\"frames\":%zu,\"setup_wall\":%.9f,\"render_wall\":%.9f,\"render_cpu\":%.9f,\"maxrss_kib\":%ld,\"hash\":\"%016llx\",\"rms\":%.9g,\"peak\":%.9g}\n",
        block, cores, fx, frames, setup_wall, render_wall, render_cpu, usage.ru_maxrss,
        (unsigned long long)hash, sqrt(energy/(2*frames)), peak);
    free(audio);
    checked(fluid_player_stop(player));
    delete_fluid_player(player);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    return 0;
}
