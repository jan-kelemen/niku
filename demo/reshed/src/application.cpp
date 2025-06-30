#include "vkrndr_swap_chain.hpp"
#include <algorithm>
#include <application.hpp>

#include <text_editor.hpp>
#include <window.hpp>

#include <cppext_container.hpp>
#include <cppext_numeric.hpp>
#include <cppext_overloaded.hpp>

#include <ngnwsi_application.hpp>
#include <ngnwsi_imgui_layer.hpp>
#include <ngnwsi_mouse.hpp>
#include <ngnwsi_sdl_window.hpp>

#include <ngntxt_font_face.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_commands.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_library.hpp>
#include <vkrndr_render_pass.hpp>
#include <vkrndr_render_settings.hpp>
#include <vkrndr_synchronization.hpp>
#include <vkrndr_utility.hpp>

#include <imgui.h>

#include <volk.h>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_video.h>

#include <spdlog/spdlog.h>

#include <exception>
#include <expected>
#include <memory>
#include <numeric>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <variant>

// IWYU pragma: no_include  <filesystem>

reshed::application_t::application_t(bool const debug)
    : ngnwsi::application_t{ngnwsi::startup_params_t{
          .init_subsystems = {.video = true, .debug = debug}}}
    , freetype_context_{ngntxt::freetype_context_t::create()}
{
}

reshed::application_t::~application_t() = default;

bool reshed::application_t::should_run() { return !windows_.empty(); }

bool reshed::application_t::handle_event(SDL_Event const& event)
{
    if (event.type == SDL_EVENT_QUIT)
    {
        vkDeviceWaitIdle(backend_->device().logical);

        std::ranges::for_each(windows_,
            [this](std::unique_ptr<window_t> const& wnd)
            { unregister_window(wnd.get()); });

        windows_.clear();
        return true;
    }

    if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
    {
        vkDeviceWaitIdle(backend_->device().logical);

        auto it{std::ranges::find_if(windows_,
            [&event](std::unique_ptr<window_t> const& wnd)
            { return wnd->id() == event.window.windowID; })};

        unregister_window(it->get());

        windows_.erase(it);
        return true;
    }

    if (auto focused_window{
            std::ranges::find_if(windows_, &window_t::is_focused)};
        focused_window != std::cend(windows_))
    {
        (*focused_window)->handle_event(event);
    }

    return true;
}

void reshed::application_t::debug_draw()
{
    std::ranges::for_each(windows_,
        [](std::unique_ptr<window_t>& window) { window->debug_draw(); });
}

bool reshed::application_t::begin_frame()
{
    bool const any_ready{std::transform_reduce(windows_.begin(),
        windows_.end(),
        false,
        std::logical_or{},
        [](std::unique_ptr<window_t>& window)
        { return window->begin_frame(); })};

    if (any_ready)
    {
        backend_->begin_frame();
        return true;
    }
    return false;
}

void reshed::application_t::draw()
{
    std::ranges::for_each(windows_,
        [](std::unique_ptr<window_t>& window) { return window->render(); });
}

void reshed::application_t::end_frame(bool const has_rendered)
{
    std::ranges::for_each(windows_,
        [](std::unique_ptr<window_t>& window) { window->end_frame(); });

    if (has_rendered)
    {
        backend_->end_frame();
    }
}

void reshed::application_t::on_startup()
{
    windows_.push_back(std::make_unique<window_t>());

    window_t* const wnd{windows_.back().get()};
    register_window(wnd);

    backend_ = std::make_unique<vkrndr::backend_t>(
        vkrndr::initialize(),
        [wnd](VkInstance instance) { return wnd->create_surface(instance); },
        vkrndr::render_settings_t{
            .required_extensions = ngnwsi::sdl_window_t::required_extensions(),
            .preferred_swapchain_format = VK_FORMAT_R8G8B8A8_UNORM,
            .swapchain_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                VK_IMAGE_USAGE_STORAGE_BIT |
                VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .preferred_present_mode = VK_PRESENT_MODE_FIFO_KHR,
        },
        true);
    wnd->init_rendering(*backend_);

    windows_.push_back(std::make_unique<window_t>());
    register_window(windows_.back().get());
    windows_.back().get()->create_surface(backend_->instance().handle);
    windows_.back().get()->init_rendering(*backend_);

    mouse_.set_window_handle(wnd->native_window().native_handle());

    if (std::expected<ngntxt::font_face_ptr_t, std::error_code> font_face{
            load_font_face(freetype_context_,
                "SpaceMono-Regular.ttf",
                {0, 16})})
    {
        spdlog::info("Font loaded");
        std::ranges::for_each(windows_,
            [&font_face](std::unique_ptr<window_t>& window)
            { window->set_font(*font_face); });
    }
    else
    {
        std::terminate();
    }
}

void reshed::application_t::on_shutdown()
{
    vkDeviceWaitIdle(backend_->device().logical);

    windows_.clear();

    backend_.reset();
}
