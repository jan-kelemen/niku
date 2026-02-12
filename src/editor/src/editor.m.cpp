#include <editor_application.hpp>

#include <cppext_numeric.hpp>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>

#include <cstddef>

// IWYU pragma: no_include <SDL_events.h>
// IWYU pragma: no_include <span>

namespace
{
    [[nodiscard]] inline editor::application_t* to_app(
        void* const appstate) noexcept
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return reinterpret_cast<editor::application_t*>(appstate);
    }
} // namespace

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

SDL_AppResult SDL_AppIterate([[maybe_unused]] void* const appstate)
{
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, [[maybe_unused]] SDL_AppResult result)
{
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    delete to_app(appstate);
}
