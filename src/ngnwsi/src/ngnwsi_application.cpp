#include <ngnwsi_application.hpp>

#include <ngnwsi_window.hpp>

#include <cppext_numeric.hpp>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>

#include <cstdint>
#include <map>
#include <memory>

namespace
{
    [[nodiscard]] constexpr uint32_t to_init_flags(
        ngnwsi::subsystems_t const& subsystems)
    {
        return subsystems.video ? SDL_INIT_VIDEO : 0;
    }
} // namespace

struct [[nodiscard]] ngnwsi::application_t::impl final
{
public:
    explicit impl(startup_params_t const& params);

    impl(impl const&) = delete;

    impl(impl&&) noexcept = delete;

public:
    ~impl();

public:
    [[nodiscard]] window_t* get_window_for_event(SDL_Event const& event) const;

public:
    impl& operator=(impl const&) = delete;

    impl& operator=(impl&&) noexcept = delete;

public:
    sdl_guard_t guard;

    std::map<uint32_t, window_t*> windows;

    float fixed_update_interval{1.0f / 60.0f};
};

ngnwsi::application_t::impl::impl(startup_params_t const& params)
    : guard{to_init_flags(params.init_subsystems)}
{
}

ngnwsi::application_t::impl::~impl() = default;

ngnwsi::window_t* ngnwsi::application_t::impl::get_window_for_event(
    SDL_Event const& event) const
{
    if (event.type >= SDL_EVENT_WINDOW_FIRST &&
        event.type <= SDL_EVENT_WINDOW_LAST)
    {
        if (auto it{windows.find(event.window.windowID)}; it != windows.end())
        {
            return it->second;
        }
    }

    return nullptr;
}

ngnwsi::application_t::application_t(startup_params_t const& params)
    : impl_{std::make_unique<impl>(params)}
{
}

ngnwsi::application_t::~application_t() = default;

void ngnwsi::application_t::run()
{
    on_startup();

    auto const frequency{cppext::as_fp(SDL_GetPerformanceFrequency())};

    uint64_t last_tick{SDL_GetPerformanceCounter()};
    float accumulated_error{};
    while (true)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (window_t* const window{impl_->get_window_for_event(event)};
                window && !is_quit_event(event))
            {
                window->handle_event(event);
            }

            handle_event(event);
        }

        if (!should_run())
        {
            break;
        }

        // https://gafferongames.com/post/fix_your_timestep/
        // https://dewitters.com/dewitters-gameloop/
        // https://jakubtomsu.github.io/posts/input_in_fixed_timestep/
        uint64_t const current_tick{SDL_GetPerformanceCounter()};
        auto const frame_duration_in_seconds{
            cppext::as_fp(current_tick - last_tick) / frequency};

        accumulated_error += frame_duration_in_seconds;
        auto const simulation_steps{static_cast<uint64_t>(
            accumulated_error / impl_->fixed_update_interval)};
        accumulated_error -=
            cppext::as_fp(simulation_steps) * impl_->fixed_update_interval;

        // spdlog::info(
        //     "last_tick {} current_tick {} duration {} steps {} error {}
        //     frequency {}", last_tick, current_tick,
        //     frame_duration_in_seconds,
        //     simulation_steps,
        //     accumulated_error,
        //     frequency);

        last_tick = current_tick;
        bool const should_render{begin_frame()};

        for (uint64_t i{}; i != simulation_steps; ++i)
        {
            update(impl_->fixed_update_interval);
        }

        if (should_render)
        {
            debug_draw();

            draw();
        }

        end_frame(should_render);
    }

    on_shutdown();
}

void ngnwsi::application_t::fixed_update_interval(float const fps)
{
    impl_->fixed_update_interval = fps;
}

void ngnwsi::application_t::register_window(window_t* const window)
{
    impl_->windows.emplace(window->id(), window);
}

void ngnwsi::application_t::unregister_window(window_t const* const window)
{
    impl_->windows.erase(window->id());
}

bool ngnwsi::application_t::is_quit_event(SDL_Event const& event)
{
    switch (event.type)
    {
    case SDL_EVENT_QUIT:
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        return true;
    default:
        return false;
    }
}
