#include "test.h"
#include "utils/fluid_sys.h"

static fluid_thread_return_t throw_exception(void *data)
{
    ++*static_cast<int *>(data);
    throw 1;
}

int main(void)
{
    int called = 0;
    fluid_thread_t *thread = new_fluid_thread("exception", throw_exception, &called, 1, FALSE);

    TEST_ASSERT(thread != NULL);
    TEST_SUCCESS(fluid_thread_join(thread));
    delete_fluid_thread(thread);
    TEST_ASSERT(called == 1);
    return EXIT_SUCCESS;
}
