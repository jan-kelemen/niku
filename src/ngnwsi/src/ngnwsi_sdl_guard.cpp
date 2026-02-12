#include <ngnwsi_sdl_guard.hpp>

#include <ngnwsi_sdl_window.hpp>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_keyboard.h>

#include <stdexcept>

ngnwsi::sdl_guard_t::sdl_guard_t(uint32_t const flags)
{
    if (!SDL_Init(flags))
    {
        throw std::runtime_error{SDL_GetError()};
    }
    SDL_SetHint(SDL_HINT_IME_IMPLEMENTED_UI, "1");
}

ngnwsi::sdl_guard_t::~sdl_guard_t() { SDL_Quit(); }

ngnwsi::sdl_text_input_guard_t::sdl_text_input_guard_t(
    sdl_window_t const& window)
    : window_{&window}
{
    if (!SDL_StartTextInput(window_->native_handle()))
    {
        throw std::runtime_error{SDL_GetError()};
    }
}

ngnwsi::sdl_text_input_guard_t::~sdl_text_input_guard_t()
{
    SDL_StopTextInput(window_->native_handle());
}
