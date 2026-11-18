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
#include "fluid_sys.h"
#include "signalsmith-basics/limiter.h"
#include <atomic>
#include <memory>

#ifdef WITH_FLOAT
using Limiter = signalsmith::basics::LimiterFloat;
#else
using Limiter = signalsmith::basics::LimiterDouble;
#endif

/* A prepared node crosses the control/render boundary exactly once. */
struct prepared_limiter
{
    std::unique_ptr<Limiter> dsp;
    prepared_limiter *retired_next = nullptr;
};

/* Control updates may lock; the renderer never takes this lock. */
struct limiter_control_lock
{
    fluid_rec_mutex_t &mutex;
    explicit limiter_control_lock(fluid_rec_mutex_t &mutex) : mutex(mutex)
    {
        fluid_rec_mutex_lock(mutex);
    }
    ~limiter_control_lock()
    {
        fluid_rec_mutex_unlock(mutex);
    }
};

struct _fluid_limiter_t
{
    fluid_limiter_settings_t settings;
    fluid_real_t sample_rate;
    unsigned int block_size;
    fluid_rec_mutex_t control_mutex;
    std::atomic<prepared_limiter *> ready{nullptr};
    /* Rendering can retire one node during preparation and one after publication.
     * Control calls reclaim them before preparing another request. */
    std::atomic<prepared_limiter *> retired{nullptr};
    prepared_limiter *current = nullptr;

    /* Initialize limiter control state. */
    _fluid_limiter_t(fluid_real_t rate, const fluid_limiter_settings_t &params,
                     unsigned int size)
        : settings(params), sample_rate(rate), block_size(size)
    {
        fluid_rec_mutex_init(control_mutex);
#if !OSAL_embedded
        if(!control_mutex)
        {
            throw std::bad_alloc();
        }
#endif
    }

    /* Rendering must have stopped before destruction, as for the synth. */
    ~_fluid_limiter_t()
    {
        reclaim();
        delete current;
        delete ready.exchange(nullptr);
        fluid_rec_mutex_destroy(control_mutex);
    }

    /* Configure only on a control thread, including allocation and resets. */
    static std::unique_ptr<prepared_limiter> prepare(const fluid_limiter_settings_t &params,
                                                    fluid_real_t rate, unsigned int size)
    {
        std::unique_ptr<prepared_limiter> node(new prepared_limiter);
        if(params.active)
        {
            /* Peak-hold rounds attack and hold separately; smoothing needs
             * one additional sample beyond their sum. */
            double samples = std::ceil(params.attack_ms * 0.001 * rate)
                           + std::ceil(params.hold_ms * 0.001 * rate) + 1;
            node->dsp.reset(new Limiter(samples * 1000 / rate));
            auto &dsp = *node->dsp;
            dsp.inputGain = params.input_gain;
            dsp.outputLimit = params.output_limit;
            dsp.attackMs = params.attack_ms;
            dsp.holdMs = params.hold_ms;
            dsp.releaseMs = params.release_ms;
            dsp.smoothingStages = params.smoothing_stages;
            dsp.linkChannels = params.link_channels;
            if(!dsp.configure(rate, size, FLUID_LIMITER_NUM_CHANNELS_AT_ONCE))
            {
                fluid_log(FLUID_WARN, "Limiter parameters were not accepted");
                return nullptr;
            }
        }
        return node;
    }

    /* Each exchange transfers an entire list to exactly one cleanup caller. */
    void reclaim()
    {
        auto node = retired.exchange(nullptr, std::memory_order_acquire);
        while(node)
        {
            auto next = node->retired_next;
            delete node;
            node = next;
        }
    }

    /* Prepare before publishing so failure leaves the previous request intact.
     * Callers hold the control lock. */
    void request(const fluid_limiter_settings_t &params,
                 fluid_real_t rate, unsigned int size)
    {
        if(params.active || settings.active)
        {
            auto next = prepare(params, rate, size);
            if(!next)
            {
                return;
            }
            delete ready.exchange(next.release(), std::memory_order_acq_rel);
        }
        settings = params;
        sample_rate = rate;
        block_size = size;
    }

    /* Only rendering pushes; control updates detach whole lists for reclamation. */
    void retire(prepared_limiter *node)
    {
        if(!node)
        {
            return;
        }
        auto head = retired.load(std::memory_order_relaxed);
        do
        {
            node->retired_next = head;
        }
        /* Other threads can only change head to null. A strong CAS therefore
         * needs at most one retry; there is no unbounded spin on the renderer. */
        while(!retired.compare_exchange_strong(head, node, std::memory_order_release, std::memory_order_relaxed));
    }

    /* Runtime settings avoid rebuilding the synth and reloading SoundFonts.
     * Reconfiguration resets the limiter history and may interrupt audio or
     * change latency; this tradeoff is accepted in issue #1890.
     * https://github.com/FluidSynth/fluidsynth/issues/1890#issuecomment-6954709411 */
    void process(fluid_real_t *bufs[FLUID_LIMITER_NUM_CHANNELS_AT_ONCE], unsigned int count)
    {
        if(ready.load(std::memory_order_acquire))
        {
            auto next = ready.exchange(nullptr, std::memory_order_acq_rel);
            if(next)
            {
                retire(current);
                current = next;
            }
        }
        if(current && current->dsp)
        {
            current->dsp->process(bufs, count);
        }
    }
};

extern "C" void fluid_limiter_impl_set_sample_rate(fluid_limiter_t* lim, fluid_real_t sample_rate, unsigned int block_size)
try
{
    limiter_control_lock lock(lim->control_mutex);
    lim->reclaim();
    if(lim->sample_rate == sample_rate && lim->block_size == block_size)
    {
        return;
    }
    lim->request(lim->settings, sample_rate, block_size);
}
catch(const std::bad_alloc &)
{
    fluid_log(FLUID_ERR, "Out of memory updating limiter sample rate");
}

extern "C" fluid_limiter_t *fluid_limiter_impl_new(fluid_real_t sample_rate, fluid_limiter_settings_t* settings, unsigned int block_size)
try
{
    std::unique_ptr<fluid_limiter_t> lim(new fluid_limiter_t(sample_rate, *settings, block_size));
    if(settings->active)
    {
        auto initial = fluid_limiter_t::prepare(*settings, sample_rate, block_size);
        if(!initial)
        {
            return nullptr;
        }
        lim->current = initial.release();
    }
    return lim.release();
}
catch(const std::bad_alloc &)
{
    fluid_log(FLUID_ERR, "Out of memory allocating limiter");
    return nullptr;
}

/* Serialize reading and applying the current settings so concurrent callbacks
 * cannot publish an older snapshot after a newer one. */
extern "C" void fluid_limiter_set(fluid_limiter_t *lim, fluid_settings_t *settings)
try
{
    limiter_control_lock lock(lim->control_mutex);
    lim->reclaim();
    fluid_limiter_settings_t params;
    fluid_limiter_get_settings(settings, &params);
    const auto &old = lim->settings;
    if(params.active == old.active && params.output_limit == old.output_limit
            && params.attack_ms == old.attack_ms && params.hold_ms == old.hold_ms
            && params.release_ms == old.release_ms && params.smoothing_stages == old.smoothing_stages
            && params.link_channels == old.link_channels)
    {
        return;
    }
    lim->request(params, lim->sample_rate, lim->block_size);
}
catch(const std::bad_alloc &)
{
    fluid_log(FLUID_ERR, "Out of memory updating limiter");
}

extern "C" void fluid_limiter_impl_delete(fluid_limiter_t* lim)
{
    delete lim;
}

extern "C" void fluid_limiter_impl_process_buffers(
    fluid_limiter_t *lim,
    fluid_real_t *bufs[FLUID_LIMITER_NUM_CHANNELS_AT_ONCE],
    unsigned int block_size)
{
    lim->process(bufs, block_size);
}

#endif /* SIGNALSMITH_SUPPORT */
