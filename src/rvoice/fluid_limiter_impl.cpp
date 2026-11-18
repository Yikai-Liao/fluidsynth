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

/* Control and cleanup may lock; the renderer never takes this lock. */
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
    bool active;
    fluid_rec_mutex_t control_mutex;
    fluid_thread_t *cleanup_thread = nullptr;
    bool cleanup_running = false;
    bool stopping = false;
    std::atomic<prepared_limiter *> ready{nullptr};
    std::atomic<prepared_limiter *> retired{nullptr};
    std::atomic<bool> audio_busy{false};
    prepared_limiter *current = nullptr;

    /* Initialize control state; no helper thread is needed for static users. */
    _fluid_limiter_t(fluid_real_t rate, const fluid_limiter_settings_t &params,
                     unsigned int size, bool enabled)
        : settings(params), sample_rate(rate), block_size(size), active(enabled)
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
        fluid_thread_t *thread;
        {
            limiter_control_lock lock(control_mutex);
            stopping = true;
            thread = cleanup_thread;
            cleanup_thread = nullptr;
        }
        if(thread)
        {
            fluid_thread_join(thread);
            delete_fluid_thread(thread);
        }
        reclaim();
        delete current;
        delete ready.exchange(nullptr);
        fluid_rec_mutex_destroy(control_mutex);
    }

    /* Configure only on a control thread, including allocation and resets. */
    static std::unique_ptr<prepared_limiter> prepare(const fluid_limiter_settings_t &params,
                                                    fluid_real_t rate, unsigned int size, bool enabled)
    {
        std::unique_ptr<prepared_limiter> node(new prepared_limiter);
        if(enabled)
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

    /* Read in this order: ready's null exchange publishes audio_busy=true;
     * audio_busy=false in turn publishes the completed retirement. */
    bool has_work() const
    {
        auto request = ready.load(std::memory_order_acquire);
        auto busy = audio_busy.load(std::memory_order_acquire);
        auto garbage = retired.load(std::memory_order_acquire);
        return request || busy || garbage;
    }

    /* Exit completely when idle; no permanent polling or idle thread remains. */
    static fluid_thread_return_t cleanup(void *data)
    {
        auto lim = static_cast<fluid_limiter_t *>(data);
        for(;;)
        {
            {
                limiter_control_lock lock(lim->control_mutex);
                if(lim->stopping)
                {
                    lim->cleanup_running = false;
                    return FLUID_THREAD_RETURN_VALUE; /* Destructor owns the handle. */
                }
                /* Serialize reclamation with control-side preparation so
                 * detached-but-not-yet-freed nodes do not inflate its bound. */
                lim->reclaim();
                if(!lim->has_work())
                {
                    lim->cleanup_running = false;
                    return FLUID_THREAD_RETURN_VALUE;
                }
            }
            fluid_msleep(5);
        }
    }

    /* Publish only fully configured nodes; the old mailbox remains recoverable
     * if allocation fails. Callers hold the control lock, never the synth lock. */
    bool request(const fluid_limiter_settings_t &params, bool enabled,
                 fluid_real_t rate, unsigned int size)
    {
        if(enabled || active)
        {
#if !OSAL_embedded
            if(!cleanup_running)
            {
                /* Keep a joinable handle even after idle exit: destruction
                 * must also wait for the thread's final library instructions. */
                if(cleanup_thread)
                {
                    fluid_thread_join(cleanup_thread);
                    delete_fluid_thread(cleanup_thread);
                    cleanup_thread = nullptr;
                }
                cleanup_running = true;
                cleanup_thread = new_fluid_thread("limiter-cleanup", cleanup, this, 0, FALSE);
                if(!cleanup_thread)
                {
                    cleanup_running = false;
                    return false;
                }
            }
#endif
            std::unique_ptr<prepared_limiter> superseded(ready.exchange(nullptr, std::memory_order_acq_rel));
            reclaim();
            try
            {
                auto next = prepare(params, rate, size, enabled);
                if(!next)
                {
                    ready.store(superseded.release(), std::memory_order_release);
                    return false;
                }
                ready.store(next.release(), std::memory_order_release);
            }
            catch(...)
            {
                ready.store(superseded.release(), std::memory_order_release);
                throw;
            }
        }
        settings = params;
        active = enabled;
        sample_rate = rate;
        block_size = size;
        return true;
    }

    /* Only the renderer pushes. Cleanup/control only detach lists, so a
     * detached head can never be pushed back (no multiple-publisher ABA). */
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
            audio_busy.store(true, std::memory_order_release);
            auto next = ready.exchange(nullptr, std::memory_order_acq_rel);
            if(next)
            {
                retire(current);
                current = next;
            }
            audio_busy.store(false, std::memory_order_release);
        }
        if(current && current->dsp)
        {
            current->dsp->process(bufs, count);
        }
    }
};

extern "C" void fluid_limiter_impl_set_sample_rate(fluid_limiter_t *lim, fluid_real_t rate, unsigned int size)
try
{
    limiter_control_lock lock(lim->control_mutex);
    if(lim->sample_rate == rate && lim->block_size == size)
    {
        return;
    }
    lim->request(lim->settings, lim->active, rate, size);
}
catch(const std::bad_alloc &)
{
    fluid_log(FLUID_ERR, "Out of memory updating limiter sample rate");
}

extern "C" fluid_limiter_t *fluid_limiter_impl_new(fluid_real_t rate, fluid_limiter_settings_t *settings, unsigned int size, int active)
try
{
    std::unique_ptr<fluid_limiter_t> lim(new fluid_limiter_t(rate, *settings, size, active != 0));
    if(active)
    {
        auto initial = fluid_limiter_t::prepare(*settings, rate, size, true);
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

/* Compare desired values on the control thread, not mutable renderer state. */
template<class T> static bool change_limiter_value(T &field, T value)
{
    if(field == value)
    {
        return false;
    }
    field = value;
    return true;
}

extern "C" void fluid_limiter_set(fluid_limiter_t *lim, enum fluid_limiter_param param, fluid_real_t value)
try
{
    limiter_control_lock lock(lim->control_mutex);
    auto settings = lim->settings;
    auto active = lim->active;
    bool changed = false;
    switch(param)
    {
    case FLUID_LIMITER_ACTIVE: changed = change_limiter_value(active, value != 0); break;
    case FLUID_LIMITER_OUTPUT_LIMIT: changed = change_limiter_value(settings.output_limit, value); break;
    case FLUID_LIMITER_ATTACK: changed = change_limiter_value(settings.attack_ms, value); break;
    case FLUID_LIMITER_HOLD: changed = change_limiter_value(settings.hold_ms, value); break;
    case FLUID_LIMITER_RELEASE: changed = change_limiter_value(settings.release_ms, value); break;
    case FLUID_LIMITER_SMOOTHING_STAGES: changed = change_limiter_value(settings.smoothing_stages, (int)value); break;
    case FLUID_LIMITER_LINK_CHANNELS: changed = change_limiter_value(settings.link_channels, value); break;
    }
    if(changed)
    {
        lim->request(settings, active, lim->sample_rate, lim->block_size);
    }
}
catch(const std::bad_alloc &)
{
    fluid_log(FLUID_ERR, "Out of memory updating limiter");
}

extern "C" void fluid_limiter_impl_delete(fluid_limiter_t *lim)
{
    delete lim;
}

extern "C" void fluid_limiter_impl_process_buffers(fluid_limiter_t *lim,
    fluid_real_t *bufs[FLUID_LIMITER_NUM_CHANNELS_AT_ONCE], unsigned int count)
{
    lim->process(bufs, count);
}

#endif /* SIGNALSMITH_SUPPORT */
