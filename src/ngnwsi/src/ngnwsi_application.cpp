#include <ngnwsi_application.hpp>

#include <ngnwsi_fixed_timestep.hpp>
#include <ngnwsi_sdl_guard.hpp>

#include <cppext_numeric.hpp>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_timer.h>

#include <cstdint>
#include <memory>
#include <span>

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
    impl& operator=(impl const&) = delete;

    impl& operator=(impl&&) noexcept = delete;

public:
    sdl_guard_t guard;
    fixed_timestep_t timestep;

    std::span<char const*> command_line_parameters;
};

ngnwsi::application_t::impl::impl(startup_params_t const& params)
    : guard{to_init_flags(params.init_subsystems)}
    , command_line_parameters{params.command_line_parameters}
{
}

ngnwsi::application_t::impl::~impl() = default;

ngnwsi::application_t::application_t(startup_params_t const& params)
    : impl_{std::make_unique<impl>(params)}
{
}

ngnwsi::application_t::~application_t() = default;

void ngnwsi::application_t::run()
{
    on_startup();

    while (true)
    {
        begin_frame();

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            handle_event(event);
        }

        if (!should_run())
        {
            break;
        }

        for (uint64_t i{}, end{impl_->timestep.pending_simulation_steps()};
            i != end;
            ++i)
        {
            update(impl_->timestep.update_interval);
        }

        draw();

        end_frame();
    }

    on_shutdown();
}

void ngnwsi::application_t::fixed_update_interval(float const ups)
{
    impl_->timestep.update_interval = 1.0f / ups;
}

std::span<char const*> ngnwsi::application_t::command_line_parameters()
{
    return impl_->command_line_parameters;
}
