#ifndef FLUID_JACK_H
#define FLUID_JACK_H

#include "fluid_sys.h"
#include <jack/jack.h>

typedef struct _fluid_jack_audio_driver_t fluid_jack_audio_driver_t;
typedef struct _fluid_jack_midi_driver_t fluid_jack_midi_driver_t;


/* Clients are shared for drivers using the same server. */
typedef struct
{
    jack_client_t *client;
    char *server;                 /* Jack server name used */
    fluid_jack_audio_driver_t *audio_driver;
    fluid_jack_midi_driver_t *midi_driver;
    fluid_atomic_int_t sample_rate;       /* Last rate reported by JACK. */
    fluid_atomic_int_t audio_sample_rate; /* Rate used to configure the audio driver. */
} fluid_jack_client_t;

int fluid_jack_driver_srate(jack_nframes_t rate, void *arg);
int fluid_jack_driver_process(jack_nframes_t frames, void *arg);

#endif
