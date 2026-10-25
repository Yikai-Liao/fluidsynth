#include "test.h"
#include <SDL3/SDL.h>
#include <fluidsynth.h>

int main(void)
{
    fluid_settings_t *settings = new_fluid_settings();
    TEST_ASSERT(settings != NULL);
    delete_fluid_settings(settings);

    if(SDL_Init(SDL_INIT_AUDIO))
    {
        settings = new_fluid_settings();
        TEST_ASSERT(settings != NULL);
        delete_fluid_settings(settings);
        SDL_Quit();
    }

    return EXIT_SUCCESS;
}
