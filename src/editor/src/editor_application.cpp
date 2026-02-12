#include <editor_application.hpp>

#include <ngnwsi_render_window.hpp>
#include <ngnwsi_sdl_window.hpp>

#include <vkrndr_instance.hpp>
#include <vkrndr_library_handle.hpp>
#include <vkrndr_rendering_context.hpp>
#include <vkrndr_utility.hpp>

#include <fmt/ranges.h>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_video.h>

#include <spdlog/common.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <expected>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

// IWYU pragma: no_include <fmt/base.h>
// IWYU pragma: no_include <fmt/format.h>
// IWYU pragma: no_include <boost/smart_ptr/intrusive_ptr.hpp>
// IWYU pragma: no_include <boost/smart_ptr/intrusive_ref_counter.hpp>
// IWYU pragma: no_include <set>

namespace
{
    constexpr auto application_name{"Niku Editor"};
} // namespace

editor::application_t::application_t(
    std::span<char const*> command_line_parameters)
    : application_t()
{
    process_command_line(command_line_parameters);

    main_window_ = std::make_unique<ngnwsi::render_window_t>(application_name,
        SDL_WINDOW_MAXIMIZED | SDL_WINDOW_RESIZABLE,
        1920,
        1080);

    std::vector<char const*> const instance_extensions{
        ngnwsi::sdl_window_t::required_extensions()};
    if (std::expected<vkrndr::instance_ptr_t, std::error_code> instance{
            vkrndr::create_instance({
                .extensions = instance_extensions,
                .application_name = application_name,
            })})
    {
        spdlog::debug(
            "Created with instance handle {}.\n\tEnabled extensions: {}\n\tEnabled layers: {}",
            vkrndr::handle_cast((*instance)->handle),
            fmt::join((*instance)->extensions, ", "),
            fmt::join((*instance)->layers, ", "));

        rendering_context_.instance = *std::move(instance);
    }
    else
    {
        auto message{instance.error().message()};
        spdlog::error("Failed to create rendering instance: {}", message);
        throw std::runtime_error{message};
    }
}

editor::application_t::~application_t()
{
    if (main_window_)
    {
        main_window_->destroy_swapchain();
        main_window_->destroy_surface(*rendering_context_.instance);
        main_window_.reset();
    }

    destroy(rendering_context_);

    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

bool editor::application_t::handle_event(SDL_Event const& event)
{
    return event.type != SDL_EVENT_QUIT;
}

bool editor::application_t::update() { return true; }

editor::application_t::application_t()
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        throw std::runtime_error{SDL_GetError()};
    }

    if (std::expected<vkrndr::library_handle_ptr_t, std::error_code> lh{
            vkrndr::initialize()})
    {
        rendering_context_.library_handle = *std::move(lh);
    }
    else
    {
        auto message{lh.error().message()};
        spdlog::error("Failed to load rendering library: {}", message);
        throw std::runtime_error{message};
    }
}

void editor::application_t::process_command_line(
    std::span<char const*> const& parameters)
{
    auto const has_argument = [&parameters](std::string_view s)
    { return std::ranges::contains(cbegin(parameters), cend(parameters), s); };

    using namespace std::string_view_literals;
    if (has_argument("--trace"))
    {
        spdlog::set_level(spdlog::level::trace);
    }
    else if (has_argument("--debug"))
    {
        spdlog::set_level(spdlog::level::debug);
    }
}
