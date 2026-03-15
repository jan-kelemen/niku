#include <ngnwsi_fixed_timestep.hpp>

#include <cppext_numeric.hpp>

#include <SDL3/SDL_timer.h>

// #include <spdlog/spdlog.h>

ngnwsi::fixed_timestep_t::fixed_timestep_t(float const ups)
    : update_interval{1.0f / ups}
    , frequency_{cppext::as_fp(SDL_GetPerformanceFrequency())}
    , last_tick_{SDL_GetPerformanceCounter()}
{
}

uint64_t ngnwsi::fixed_timestep_t::pending_simulation_steps()
{
    // https://gafferongames.com/post/fix_your_timestep/
    // https://dewitters.com/dewitters-gameloop/
    // https://jakubtomsu.github.io/posts/input_in_fixed_timestep/
    uint64_t const current_tick{SDL_GetPerformanceCounter()};
    auto const frame_duration_in_seconds{
        cppext::as_fp(current_tick - last_tick_) / frequency_};

    accumulated_error_ += frame_duration_in_seconds;
    auto const simulation_steps{
        static_cast<uint64_t>(accumulated_error_ / update_interval)};
    accumulated_error_ -= cppext::as_fp(simulation_steps) * update_interval;

    // spdlog::info(
    //     "last_tick {} current_tick {} duration {} steps {} error {} frequency
    //     {}", last_tick_, current_tick, frame_duration_in_seconds,
    //     simulation_steps,
    //     accumulated_error_,
    //     frequency_);

    last_tick_ = current_tick;

    return simulation_steps;
}
