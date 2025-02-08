#include <ngnwsi_mouse.hpp>

#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_video.h>

ngnwsi::mouse_t::mouse_t() : mouse_t(true) { }

ngnwsi::mouse_t::mouse_t(bool const capture) : captured_{capture}
{
    set_capture(capture);
}

void ngnwsi::mouse_t::set_window_handle(void* const window)
{
    window_handle_ = window;
}

glm::vec2 ngnwsi::mouse_t::position() const
{
    glm::vec2 rv;
    SDL_GetMouseState(&rv.x, &rv.y);
    return rv;
}

glm::vec2 ngnwsi::mouse_t::relative_offset() const
{
    glm::vec2 rv;
    SDL_GetRelativeMouseState(&rv.x, &rv.y);
    return rv;
}

bool ngnwsi::mouse_t::captured() const { return captured_; }

void ngnwsi::mouse_t::set_capture(bool const value)
{
    SDL_SetWindowRelativeMouseMode(static_cast<SDL_Window*>(window_handle_),
        value);
    captured_ = value;

    if (value)
    {
        SDL_GetRelativeMouseState(nullptr, nullptr);
    }
    else
    {
        SDL_SetWindowRelativeMouseMode(static_cast<SDL_Window*>(window_handle_),
            false);
    }
}
