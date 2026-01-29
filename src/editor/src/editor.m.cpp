#include <editor_application.hpp>

#include <cppext_numeric.hpp>

#include <SDL3/SDL_init.h>

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>

union SDL_Event;

inline [[nodiscard]] editor::application_t* to_app(
    void* const appstate) noexcept
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return reinterpret_cast<editor::application_t*>(appstate);
}

SDL_AppResult
SDL_AppInit(void** const appstate, int const argc, char** const argv)
{
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    *appstate = new editor::application_t{
        {const_cast<char const**>(argv), cppext::narrow<size_t>(argc)}};
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* const appstate, SDL_Event* const event)
{
    if (to_app(appstate)->handle_event(*event))
    {
        return SDL_APP_CONTINUE;
    }

    return SDL_APP_SUCCESS;
}

SDL_AppResult SDL_AppIterate(void* const appstate) { return SDL_APP_CONTINUE; }

void SDL_AppQuit(void* appstate, SDL_AppResult result)
{
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    delete to_app(appstate);
}
