#include <ngnwsi_render_window.hpp>

#include <ngnwsi_sdl_window.hpp>

#include <vkrndr_cpu_pacing.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_swapchain.hpp>

#include <SDL3/SDL_video.h>

#include <spdlog/spdlog.h>

ngnwsi::render_window_t::render_window_t(char const* title,
    SDL_WindowFlags window_flags,
    int width,
    int height)
    : window_{title, window_flags, width, height}
{
}

ngnwsi::render_window_t::~render_window_t() = default;

VkSurfaceKHR ngnwsi::render_window_t::create_surface(VkInstance instance)
{
    return window_.create_surface(instance);
}

void ngnwsi::render_window_t::destroy_surface(VkInstance instance)
{
    window_.destroy_surface(instance);
}

vkrndr::swapchain_t* ngnwsi::render_window_t::create_swapchain(
    vkrndr::device_t& device,
    vkrndr::swapchain_settings_t const& settings)
{
    pacing_ = std::make_unique<vkrndr::cpu_pacing_t>(device,
        settings.frames_in_flight);

    swapchain_ =
        std::make_unique<vkrndr::swapchain_t>(window_, device, settings);

    device_ = &device;

    return swapchain_.get();
}

void ngnwsi::render_window_t::destroy_swapchain() { swapchain_.reset(); }

vkrndr::frame_in_flight_t& ngnwsi::render_window_t::frame_in_flight()
{
    return pacing_->current();
}

std::optional<vkrndr::image_t> ngnwsi::render_window_t::acquire_next_image()
{
    if (VkExtent2D const extent{window_.swap_extent()};
        window_.is_minimized() || extent.height == 0 || extent.width == 0)
    {
        return std::nullopt;
    }

    vkrndr::frame_in_flight_t const* const current{pacing_->pace()};
    if (!current)
    {
        return std::nullopt;
    }

    if (swapchain_->needs_refresh())
    {
        spdlog::info("Recreating swapchain");
        swapchain_->recreate(current->index);
    }

    std::optional<vkrndr::image_t> rv{
        swapchain_->acquire_next_image(current->index)};
    if (rv)
    {
        pacing_->begin_frame();
    }

    return rv;
}

void ngnwsi::render_window_t::present(
    std::span<VkCommandBuffer const> const& command_buffers)
{
    vkrndr::frame_in_flight_t const& current{pacing_->current()};
    swapchain_->submit_command_buffers(command_buffers,
        current.index,
        current.submit_fence);

    pacing_->end_frame();
}

bool ngnwsi::render_window_t::resize([[maybe_unused]] uint32_t const width,
    [[maybe_unused]] uint32_t const height)
{
    if (width == 0 || height == 0)
    {
        return false;
    }

    spdlog::info("Recreating swapchain due to resize");
    vkrndr::frame_in_flight_t const& current{pacing_->current()};
    swapchain_->recreate(current.index);
    return true;
}
