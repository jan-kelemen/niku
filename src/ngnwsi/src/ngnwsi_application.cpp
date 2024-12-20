#include <ngnwsi_application.hpp>

#include <ngnwsi_sdl_window.hpp>

#include <cppext_numeric.hpp>

#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_video.h>

#include <cstdint>
#include <memory>
#include <optional>

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
    [[nodiscard]] bool is_current_window_event(SDL_Event const& event) const;

public:
    impl& operator=(impl const&) = delete;

    impl& operator=(impl&&) noexcept = delete;

public:
    sdl_guard_t guard;
    sdl_window_t window;

    std::optional<float> fixed_update_interval;
    uint64_t last_tick{};
    uint64_t last_fixed_tick{};
};

ngnwsi::application_t::impl::impl(startup_params_t const& params)
    : guard{to_init_flags(params.init_subsystems)}
    , window{params.title.c_str(),
          params.window_flags,
          params.centered,
          params.width,
          params.height}
{
}

ngnwsi::application_t::impl::~impl() = default;

bool ngnwsi::application_t::impl::is_current_window_event(
    SDL_Event const& event) const
{
    if (event.type == SDL_WINDOWEVENT)
    {
        return event.window.windowID == SDL_GetWindowID(window.native_handle());
    }

    return true;
}

ngnwsi::application_t::application_t(startup_params_t const& params)
    : impl_{std::make_unique<impl>(params)}
{
}

ngnwsi::application_t::~application_t() = default;

void ngnwsi::application_t::run()
{
    on_startup();

    uint64_t last_tick{SDL_GetPerformanceCounter()};
    uint64_t last_fixed_tick{last_tick};

    bool done{false};
    while (!done && should_run())
    {
        auto const frequency{cppext::as_fp(SDL_GetPerformanceFrequency())};

        uint64_t const current_tick{SDL_GetPerformanceCounter()};

        float const delta{cppext::as_fp(current_tick - last_tick) / frequency};

        SDL_Event event;
        while (SDL_PollEvent(&event) != 0)
        {
            if (impl_->is_current_window_event(event))
            {
                if (is_quit_event(event))
                {
                    done = true;
                }
                else
                {
                    handle_event(event, delta);
                }
            }
        }

        if (done)
        {
            break;
        }

        float const fixed_delta{
            cppext::as_fp(current_tick - last_fixed_tick) / frequency};

        // NOLINTBEGIN(bugprone-unchecked-optional-access)
        bool const do_fixed_update{impl_->fixed_update_interval.has_value() &&
            fixed_delta >= *impl_->fixed_update_interval};
        // NOLINTEND(bugprone-unchecked-optional-access)

        last_tick = current_tick;
        if (begin_frame())
        {
            if (do_fixed_update)
            {
                last_fixed_tick = current_tick;
                fixed_update(fixed_delta);
            }

            update(delta);

            draw();

            end_frame();
        }
    }

    on_shutdown();
}

void ngnwsi::application_t::fixed_update_interval(float const delta_time)
{
    if (delta_time != 0.0f)
    {
        impl_->fixed_update_interval = delta_time;
    }
    else
    {
        impl_->fixed_update_interval.reset();
    }
}

float ngnwsi::application_t::fixed_update_interval() const
{
    return impl_->fixed_update_interval.value_or(0.0f);
}

ngnwsi::sdl_window_t* ngnwsi::application_t::window() { return &impl_->window; }

bool ngnwsi::application_t::is_quit_event(SDL_Event const& event)
{
    switch (event.type)
    {
    case SDL_QUIT:
        return true;
    case SDL_WINDOWEVENT:
        return event.window.event == SDL_WINDOWEVENT_CLOSE;
    default:
        return false;
    }
}
