#include <niku_application.hpp>

#include <niku_sdl_window.hpp>

#include <cppext_numeric.hpp>
#include <cppext_overloaded.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_scene.hpp>

#include <imgui_impl_sdl2.h>

#include <vulkan/vulkan_core.h>

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
        niku::subsystems_t const& subsystems)
    {
        return (subsystems.video ? SDL_INIT_VIDEO : 0) |
            (subsystems.audio ? SDL_INIT_AUDIO : 0);
    }
} // namespace

struct [[nodiscard]] niku::application_t::impl final
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

    std::unique_ptr<vkrndr::backend_t> backend;

    std::optional<float> fixed_update_interval;
    uint64_t last_tick{};
    uint64_t last_fixed_tick{};
};

niku::application_t::impl::impl(startup_params_t const& params)
    : guard{to_init_flags(params.init_subsystems)}
    , window{params.title,
          params.window_flags,
          params.centered,
          params.width,
          params.height}
    , backend{std::make_unique<vkrndr::backend_t>(&window,
          params.render,
          params.init_subsystems.debug)}
{
    backend->imgui_layer(params.init_subsystems.debug);
}

niku::application_t::impl::~impl() { backend.reset(); }

bool niku::application_t::impl::is_current_window_event(
    SDL_Event const& event) const
{
    if (event.type == SDL_WINDOWEVENT)
    {
        return event.window.windowID == SDL_GetWindowID(window.native_handle());
    }

    return true;
}

niku::application_t::application_t(startup_params_t const& params)
    : impl_{std::make_unique<impl>(params)}
{
}

niku::application_t::~application_t() = default;

void niku::application_t::run()
{
    on_startup();

    uint64_t last_tick{SDL_GetPerformanceCounter()};
    uint64_t last_fixed_tick{last_tick};

    bool done{false};
    while (!done && should_run())
    {
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0)
        {
            if (debug_layer())
            {
                ImGui_ImplSDL2_ProcessEvent(&event);
            }

            if (impl_->is_current_window_event(event))
            {
                if (is_quit_event(event))
                {
                    done = true;
                }
                else
                {
                    handle_event(event);
                }
            }
        }

        if (done)
        {
            break;
        }

        auto const frequency{cppext::as_fp(SDL_GetPerformanceFrequency())};

        uint64_t const current_tick{SDL_GetPerformanceCounter()};

        float const delta{cppext::as_fp(current_tick - last_tick) / frequency};

        float const fixed_delta{
            cppext::as_fp(current_tick - last_fixed_tick) / frequency};

        // NOLINTBEGIN(bugprone-unchecked-optional-access)
        bool const do_fixed_update{impl_->fixed_update_interval.has_value() &&
            fixed_delta >= *impl_->fixed_update_interval};
        // NOLINTEND(bugprone-unchecked-optional-access)

        last_tick = current_tick;

        begin_frame();

        if (do_fixed_update)
        {
            last_fixed_tick = current_tick;
            fixed_update(fixed_delta);
        }

        update(delta);

        // clang-format off
        std::visit(cppext::overloaded{
            [this](vkrndr::image_t const& target_image)
            {
                vkrndr::scene_t* const scene{render_scene()};
                impl_->backend->draw(*scene, target_image);
                impl_->backend->end_frame();
            },
            [this](VkExtent2D const extent) { on_resize(extent.width, extent.height); },
            [](auto) {}},
            impl_->backend->begin_frame());
        // clang-format on

        end_frame();
    }

    vkDeviceWaitIdle(impl_->backend->device().logical);

    on_shutdown();
}

void niku::application_t::fixed_update_interval(float const delta_time)
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

float niku::application_t::fixed_update_interval() const
{
    return impl_->fixed_update_interval.value_or(0.0f);
}

void niku::application_t::debug_layer(bool const enable)
{
    impl_->backend->imgui_layer(enable);
}

bool niku::application_t::debug_layer() const
{
    return impl_->backend->imgui_layer();
}

vkrndr::device_t* niku::application_t::vulkan_device()
{
    return &impl_->backend->device();
}

vkrndr::backend_t* niku::application_t::vulkan_backend()
{
    return impl_->backend.get();
}

bool niku::application_t::is_quit_event(SDL_Event const& event)
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
