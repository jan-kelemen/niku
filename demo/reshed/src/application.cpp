#include <application.hpp>

#include <editor_window.hpp>

#include <cppext_container.hpp>

#include <ngnwsi_application.hpp>
#include <ngnwsi_render_window.hpp>
#include <ngnwsi_sdl_window.hpp>

#include <ngntxt_font_face.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_error_code.hpp>
#include <vkrndr_execution_port.hpp>
#include <vkrndr_features.hpp>
#include <vkrndr_instance.hpp>
#include <vkrndr_library_handle.hpp>

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <volk.h>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <exception>
#include <expected>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

// IWYU pragma: no_include <boost/smart_ptr/intrusive_ref_counter.hpp>
// IWYU pragma: no_include <fmt/base.h>
// IWYU pragma: no_include <filesystem>
// IWYU pragma: no_include <functional>
// IWYU pragma: no_include <map>
// IWYU pragma: no_include <string>

reshed::application_t::application_t(bool const debug)
    : ngnwsi::application_t{ngnwsi::startup_params_t{
          .init_subsystems = {.video = true, .debug = debug}}}
    , freetype_context_{ngntxt::freetype_context_t::create()}
{
    auto render_window{std::make_unique<ngnwsi::render_window_t>("reshed",
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY,
        512,
        512)};

    auto const create_result{vkrndr::initialize()
            .and_then(
                [this](vkrndr::library_handle_ptr_t&& library_handle)
                {
                    rendering_context_.library_handle =
                        std::move(library_handle);

                    std::vector<char const*> const instance_extensions{
                        ngnwsi::sdl_window_t::required_extensions()};

                    return vkrndr::create_instance({
                        .extensions = instance_extensions,
                        .application_name = "reshed",
                    });
                })
            .transform(
                [this](vkrndr::instance_ptr_t&& instance)
                {
                    spdlog::info(
                        "Created with instance handle {}.\n\tEnabled extensions: {}\n\tEnabled layers: {}",
                        std::bit_cast<intptr_t>(instance->handle),
                        fmt::join(instance->extensions, ", "),
                        fmt::join(instance->layers, ", "));

                    rendering_context_.instance = std::move(instance);
                })
            .transform_error(
                [](std::error_code ec)
                {
                    spdlog::error("Instance creation failed with: {}",
                        ec.message());
                    return ec;
                })
            .and_then(
                [this, &render_window]()
                    -> std::expected<vkrndr::device_ptr_t, std::error_code>
                {
                    std::array const device_extensions{
                        VK_KHR_SWAPCHAIN_EXTENSION_NAME};

                    std::optional<vkrndr::physical_device_features_t> const
                        physical_device{pick_best_physical_device(
                            *rendering_context_.instance,
                            device_extensions,
                            render_window->create_surface(
                                *rendering_context_.instance))};
                    if (!physical_device)
                    {
                        spdlog::error("No suitable physical device");
                        return std::unexpected{vkrndr::make_error_code(
                            VK_ERROR_INITIALIZATION_FAILED)};
                    }

                    auto const queue_with_present{std::ranges::find_if(
                        physical_device->queue_families,
                        [](vkrndr::queue_family_t const& f)
                        {
                            return f.supports_present &&
                                vkrndr::supports_flags(f.properties.queueFlags,
                                    VK_QUEUE_GRAPHICS_BIT);
                        })};
                    if (queue_with_present ==
                        std::cend(physical_device->queue_families))
                    {
                        spdlog::error("No present queue");
                        return std::unexpected{vkrndr::make_error_code(
                            VK_ERROR_INITIALIZATION_FAILED)};
                    }

                    return create_device(*rendering_context_.instance,
                        device_extensions,
                        *physical_device,
                        cppext::as_span(*queue_with_present));
                })
            .transform(
                [this](vkrndr::device_ptr_t&& device)
                {
                    spdlog::info(
                        "Created with device handle {}.\n\tEnabled extensions: {}",
                        std::bit_cast<intptr_t>(device->logical_device),
                        fmt::join(device->extensions, ", "));

                    rendering_context_.device = std::move(device);
                })
            .transform_error(
                [](std::error_code ec)
                {
                    spdlog::error("Device creation failed with: {}",
                        ec.message());
                    return ec;
                })
            .and_then(
                [this, &render_window]() mutable
                {
                    windows_.emplace_back(std::move(render_window),
                        rendering_context_,
                        2);

                    windows_.emplace_back(rendering_context_, 2);

                    return std::expected<std::void_t<>, std::error_code>{};
                })};
    if (!create_result)
    {
        throw std::system_error{create_result.error()};
    }
}

reshed::application_t::~application_t() = default;

bool reshed::application_t::should_run() { return !windows_.empty(); }

bool reshed::application_t::handle_event(SDL_Event const& event)
{
    if (event.type == SDL_EVENT_QUIT)
    {
        for (editor_window_t& window : windows_)
        {
            [[maybe_unused]] bool const handled{window.handle_event(
                {.window = {
                     .type = SDL_EVENT_WINDOW_CLOSE_REQUESTED,
                     .windowID = window.window_id(),
                 }})};
        }

        windows_.clear();
        return true;
    }

    if (event.type >= SDL_EVENT_WINDOW_FIRST &&
        event.type <= SDL_EVENT_WINDOW_LAST)
    {
        auto window{std::ranges::find(windows_,
            event.window.windowID,
            &editor_window_t::window_id)};
        if (window != windows_.cend())
        {
            bool const handled{window->handle_event(event)};
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
            {
                windows_.erase(window);
            }

            return handled;
        }

        return false;
    }

    if (event.type == SDL_EVENT_KEY_DOWN)
    {
        auto window{
            std::ranges::find_if(windows_, &editor_window_t::is_focused)};
        if (window != windows_.cend())
        {
            return window->handle_event(event);
        }
    }

    auto window{std::ranges::find_if(windows_, &editor_window_t::is_focused)};
    if (window != windows_.cend())
    {
        return window->handle_event(event);
    }

    return false;
}

void reshed::application_t::draw()
{
    std::ranges::for_each(windows_, &editor_window_t::draw);
}

void reshed::application_t::end_frame() { }

void reshed::application_t::on_startup()
{
    if (std::expected<ngntxt::font_face_ptr_t, std::error_code> font_face{
            load_font_face(freetype_context_,
                "SpaceMono-Regular.ttf",
                {0, 16})})
    {
        spdlog::info("Font loaded");
        for (editor_window_t& window : windows_)
        {
            window.use_font(*font_face);
        }
    }
    else
    {
        std::terminate();
    }
}

void reshed::application_t::on_shutdown()
{
    vkDeviceWaitIdle(*rendering_context_.device);

    for (editor_window_t& window : windows_)
    {
        [[maybe_unused]] bool const handled{
            window.handle_event({.window = {
                                     .type = SDL_EVENT_WINDOW_CLOSE_REQUESTED,
                                     .windowID = window.window_id(),
                                 }})};
    }
    windows_.clear();

    rendering_context_.device.reset();
    rendering_context_.instance.reset();
    rendering_context_.library_handle.reset();
}
