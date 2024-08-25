#include <niku_mouse.hpp>

#include <SDL2/SDL_mouse.h>
#include <SDL2/SDL_stdinc.h>

niku::mouse::mouse() : mouse(true) { }

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
niku::mouse::mouse(bool const capture) : captured_{capture}
{
    set_capture(capture);
}

glm::ivec2 niku::mouse::position() const
{
    glm::ivec2 rv;
    SDL_GetMouseState(&rv.x, &rv.y);
    return rv;
}

glm::ivec2 niku::mouse::relative_offset() const
{
    glm::ivec2 rv;
    SDL_GetRelativeMouseState(&rv.x, &rv.y);
    return rv;
}

bool niku::mouse::captured() const { return captured_; }

void niku::mouse::set_capture(bool const value)
{
    SDL_SetRelativeMouseMode(value ? SDL_TRUE : SDL_FALSE);
    captured_ = value;

    if (value)
    {
        SDL_GetRelativeMouseState(nullptr, nullptr);
    }
    else
    {
        SDL_SetRelativeMouseMode(SDL_FALSE);
    }
}
