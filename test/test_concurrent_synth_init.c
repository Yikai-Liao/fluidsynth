#include "test.h"
#include "synth/fluid_synth.h"
#include "synth/fluid_mod.h"
#include "utils/fluid_sys.h"

enum { ThreadCount = 8 };

struct start_gate
{
    fluid_cond_mutex_t *mutex;
    fluid_cond_t condition;
    int ready;
};

struct synth_data
{
    struct start_gate *gate;
    fluid_settings_t *settings;
    fluid_synth_t *synth;
};

static fluid_thread_return_t create_synth(void *arg)
{
    struct synth_data *data = arg;
    struct start_gate *gate = data->gate;

    fluid_cond_mutex_lock(gate->mutex);
    gate->ready++;
    fluid_cond_broadcast(gate->condition);
    while(gate->ready != ThreadCount)
    {
        fluid_cond_wait(gate->condition, gate->mutex);
    }
    fluid_cond_mutex_unlock(gate->mutex);

    data->synth = new_fluid_synth(data->settings);
    TEST_ASSERT(data->synth != NULL);
    return FLUID_THREAD_RETURN_VALUE;
}

int main(void)
{
    struct start_gate gate = {0};
    struct synth_data data[ThreadCount];
    fluid_thread_t *threads[ThreadCount];
    fluid_settings_t *reference_settings;
    fluid_synth_t *reference;
    int i;

    /* Initialize the unrelated relative clock before any concurrent constructors. */
    fluid_curtime();
    gate.mutex = new_fluid_cond_mutex();
    gate.condition = new_fluid_cond();
    TEST_ASSERT(gate.mutex != NULL && gate.condition != NULL);

    for(i = 0; i < ThreadCount; i++)
    {
        data[i].gate = &gate;
        data[i].settings = new_fluid_settings();
        TEST_ASSERT(data[i].settings != NULL);
        threads[i] = new_fluid_thread("synth-init", create_synth, &data[i], 0, FALSE);
        TEST_ASSERT(threads[i] != NULL);
    }
    for(i = 0; i < ThreadCount; i++)
    {
        fluid_thread_join(threads[i]);
        delete_fluid_thread(threads[i]);
    }

    reference_settings = new_fluid_settings();
    TEST_ASSERT(reference_settings != NULL);
    reference = new_fluid_synth(reference_settings);
    TEST_ASSERT(reference != NULL);
    TEST_ASSERT(reference->default_mod != NULL);

    for(i = 0; i < ThreadCount; i++)
    {
        fluid_mod_t *actual = data[i].synth->default_mod;
        fluid_mod_t *expected = reference->default_mod;

        for(; actual != NULL && expected != NULL; actual = actual->next, expected = expected->next)
        {
            TEST_ASSERT(fluid_mod_test_identity(actual, expected));
            TEST_ASSERT(actual->amount == expected->amount);
            TEST_ASSERT(actual->trans == expected->trans);
        }
        TEST_ASSERT(actual == NULL && expected == NULL);
        delete_fluid_synth(data[i].synth);
        delete_fluid_settings(data[i].settings);
    }

    delete_fluid_synth(reference);
    delete_fluid_settings(reference_settings);
    delete_fluid_cond(gate.condition);
    delete_fluid_cond_mutex(gate.mutex);
    return EXIT_SUCCESS;
}
