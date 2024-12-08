#include <ngnwsi_mouse.hpp>

#include <SDL2/SDL_mouse.h>
#include <SDL2/SDL_stdinc.h>

ngnwsi::mouse_t::mouse_t() : mouse_t(true) { }

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
ngnwsi::mouse_t::mouse_t(bool const capture) : captured_{capture}
{
    set_capture(capture);
}

glm::ivec2 ngnwsi::mouse_t::position() const
{
    glm::ivec2 rv;
    SDL_GetMouseState(&rv.x, &rv.y);
    return rv;
}

glm::ivec2 ngnwsi::mouse_t::relative_offset() const
{
    glm::ivec2 rv;
    SDL_GetRelativeMouseState(&rv.x, &rv.y);
    return rv;
}

bool ngnwsi::mouse_t::captured() const { return captured_; }

void ngnwsi::mouse_t::set_capture(bool const value)
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
