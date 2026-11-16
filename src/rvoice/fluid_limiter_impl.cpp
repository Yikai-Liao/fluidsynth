/* FluidSynth - A Software Synthesizer
 *
 * Copyright (C) 2003  Peter Hanappe and others.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see
 * <https://www.gnu.org/licenses/>.
 */

#include "fluidsynth_priv.h"

#ifdef SIGNALSMITH_SUPPORT

#include "fluidsynth.h"
#include "fluid_limiter.h"
#include "fluid_limiter_impl.h"
#include "signalsmith-basics/limiter.h"
#include <algorithm>
#include <memory>

#ifdef WITH_FLOAT
using Limiter = signalsmith::basics::LimiterFloat;
#else
using Limiter = signalsmith::basics::LimiterDouble;
#endif

/* The renderer owns both pending parameters and the DSP. Applying once per
 * run coalesces queued changes and keeps allocation proportional to the actual
 * lookahead, rather than reserving the maximum range for every instance. */
struct FluidLimiter
{
    fluid_limiter_settings_t settings;
    fluid_real_t sample_rate;
    unsigned int block_size;
    bool active;
    bool dirty = true;
    std::unique_ptr<Limiter> processor{};
    std::unique_ptr<Limiter> pending{};
    int warmup = 0;
    int fade_remaining = 0;
    int fade_length = 0;

    static void assign(Limiter &lim, const fluid_limiter_settings_t &settings)
    {
        lim.inputGain = settings.input_gain;
        lim.outputLimit = settings.output_limit;
        lim.attackMs = settings.attack_ms;
        lim.holdMs = settings.hold_ms;
        lim.releaseMs = settings.release_ms;
        lim.smoothingStages = settings.smoothing_stages;
        lim.linkChannels = settings.link_channels;
    }

    std::unique_ptr<Limiter> create_processor()
    {
        /* Peak-hold adds independently rounded attack and hold lengths;
         * the smoothing filter needs one further sample. */
        double samples = std::ceil(settings.attack_ms * 0.001 * sample_rate)
                       + std::ceil(settings.hold_ms * 0.001 * sample_rate) + 1;
        std::unique_ptr<Limiter> prepared(new Limiter(samples * 1000 / sample_rate));
        assign(*prepared, settings);
        if(!prepared->configure(sample_rate, block_size, FLUID_LIMITER_NUM_CHANNELS_AT_ONCE))
        {
            fluid_log(FLUID_WARN, "Limiter parameters were not accepted");
            return nullptr;
        }
        return prepared;
    }

    void apply()
    {
        dirty = false;
        if(active && (!processor || processor->attackMs != settings.attack_ms ||
                processor->holdMs != settings.hold_ms || processor->config.sampleRate != sample_rate))
        {
            pending = create_processor();
            if(!pending)
            {
                return;
            }
            /* Keep the old output audible until the new delay line is full.
             * A fresh DSP's initial zeros must never enter the crossfade. */
            warmup = pending->latencySamples();
            fade_length = (int)std::ceil(sample_rate * pending->paramFadeMs() * 0.001);
            fade_remaining = fade_length;
        }
        else if(!active && processor)
        {
            /* A null pending DSP represents the dry bypass path. */
            fade_length = (int)std::ceil(sample_rate * processor->paramFadeMs() * 0.001);
            fade_remaining = fade_length;
        }
        else if(processor)
        {
            assign(*processor, settings);
        }
    }

    void process(fluid_real_t *bufs[FLUID_LIMITER_NUM_CHANNELS_AT_ONCE], unsigned int count)
    {
        if(!fade_remaining)
        {
            if(processor)
            {
                processor->process(bufs, count);
            }
            return;
        }
        fluid_real_t samples[FLUID_LIMITER_NUM_CHANNELS_AT_ONCE][FLUID_BUFSIZE];
        fluid_real_t *next_bufs[FLUID_LIMITER_NUM_CHANNELS_AT_ONCE];
        for(int channel = 0; channel < FLUID_LIMITER_NUM_CHANNELS_AT_ONCE; channel++)
        {
            next_bufs[channel] = samples[channel];
            std::copy_n(bufs[channel], count, next_bufs[channel]);
        }
        if(processor)
        {
            processor->process(bufs, count);
        }
        if(pending)
        {
            pending->process(next_bufs, count);
        }
        if(warmup > 0)
        {
            warmup = std::max(0, warmup - (int)count);
            return;
        }
        for(unsigned int i = 0; i < count; i++)
        {
            double mix = std::min(1.0, (double)(fade_length - fade_remaining + i + 1) / fade_length);
            for(int channel = 0; channel < FLUID_LIMITER_NUM_CHANNELS_AT_ONCE; channel++)
            {
                bufs[channel][i] += mix * (next_bufs[channel][i] - bufs[channel][i]);
            }
        }
        fade_remaining = std::max(0, fade_remaining - (int)count);
        if(!fade_remaining)
        {
            processor = std::move(pending);
        }
    }
};

extern "C" void fluid_limiter_impl_set_sample_rate(fluid_limiter_t* lim, fluid_real_t sample_rate, unsigned int block_size)
{
    auto limiter = (FluidLimiter *)lim;
    limiter->sample_rate = sample_rate;
    limiter->block_size = block_size;
    limiter->dirty = true;
}

extern "C" fluid_limiter_t *fluid_limiter_impl_new(fluid_real_t sample_rate, fluid_limiter_settings_t* settings, unsigned int block_size, int active)
try
{
    std::unique_ptr<FluidLimiter> lim(new FluidLimiter{*settings, sample_rate, block_size, active != 0});
    if(active)
    {
        lim->processor = lim->create_processor();
    }
    lim->dirty = false;
    return lim.release();
}
catch(const std::bad_alloc &)
{
    fluid_log(FLUID_ERR, "Out of memory allocating limiter");
    return nullptr;
}

extern "C" void fluid_limiter_set(fluid_limiter_t *lim, enum fluid_limiter_param param, fluid_real_t value)
{
    auto limiter = (FluidLimiter *)lim;
    auto &settings = limiter->settings;
    switch(param)
    {
    case FLUID_LIMITER_ACTIVE: limiter->active = value != 0; break;
    case FLUID_LIMITER_OUTPUT_LIMIT: settings.output_limit = value; break;
    case FLUID_LIMITER_ATTACK: settings.attack_ms = value; break;
    case FLUID_LIMITER_HOLD: settings.hold_ms = value; break;
    case FLUID_LIMITER_RELEASE: settings.release_ms = value; break;
    case FLUID_LIMITER_SMOOTHING_STAGES: settings.smoothing_stages = (int)value; break;
    case FLUID_LIMITER_LINK_CHANNELS: settings.link_channels = value; break;
    }
    limiter->dirty = true;
}

extern "C" void fluid_limiter_impl_delete(fluid_limiter_t* lim)
{
    delete(FluidLimiter*)lim;
}

extern "C" void fluid_limiter_impl_process_buffers(
    fluid_limiter_t *lim,
    fluid_real_t *bufs[FLUID_LIMITER_NUM_CHANNELS_AT_ONCE],
    unsigned int block_size)
{
    auto limiter = (FluidLimiter *)lim;
    /* Serialize transitions and coalesce requests arriving during a fade.
     * There can be at most two DSP instances, even under rapid automation. */
    if(limiter->dirty && !limiter->fade_remaining)
    {
        try
        {
            limiter->apply();
        }
        catch(const std::bad_alloc &)
        {
            /* Keep the previous DSP on failure; retry only on a new change. */
            fluid_log(FLUID_ERR, "Out of memory updating limiter");
        }
    }
    limiter->process(bufs, block_size);
}

#endif /* SIGNALSMITH_SUPPORT */
