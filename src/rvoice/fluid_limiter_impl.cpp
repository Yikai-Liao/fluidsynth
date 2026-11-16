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

#include "config.h"

#if defined(WITH_FLOAT)
using fluid_real_t = float;
#else
using fluid_real_t = double;
#endif

#ifdef SIGNALSMITH_SUPPORT

#include "fluidsynth.h"
#include "fluid_limiter.h"
#include "fluid_limiter_impl.h"
#include "signalsmith-basics/limiter.h"
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

    void apply()
    {
        dirty = false;
        if(!active)
        {
            processor.reset();
            return;
        }
        if(!processor || processor->attackMs != settings.attack_ms ||
                processor->holdMs != settings.hold_ms || processor->config.sampleRate != sample_rate)
        {
            /* Peak-hold adds independently rounded attack and hold lengths;
             * the smoothing filter needs one further sample. */
            double samples = std::ceil(settings.attack_ms * 0.001 * sample_rate)
                           + std::ceil(settings.hold_ms * 0.001 * sample_rate) + 1;
            std::unique_ptr<Limiter> prepared(new Limiter(samples * 1000 / sample_rate));
            assign(*prepared, settings);
            if(!prepared->configure(sample_rate, block_size,
                                    FLUID_LIMITER_NUM_CHANNELS_AT_ONCE))
            {
                fluid_log(FLUID_WARN, "Limiter parameters were not accepted");
                return;
            }
            processor = std::move(prepared);
        }
        else
        {
            assign(*processor, settings);
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
    lim->apply();
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
    if(limiter->dirty)
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
    if(limiter->processor)
    {
        limiter->processor->process(bufs, block_size);
    }
}

#endif /* SIGNALSMITH_SUPPORT */
