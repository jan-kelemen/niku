#include <ngnwsi_render_window.hpp>

#include <vkrndr_device.hpp>
#include <vkrndr_utility.hpp>

#include <spdlog/spdlog.h>

ngnwsi::render_window_t::render_window_t(char const* title,
    SDL_WindowFlags window_flags,
    int width,
    int height)
    : window_{title, window_flags, width, height}
{
}

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
    swapchain_ =
        std::make_unique<vkrndr::swapchain_t>(window_, device, settings);

    device_ = &device;
    frames_in_flight_ = settings.frames_in_flight;

    return swapchain_.get();
}

void ngnwsi::render_window_t::destroy_swapchain() { swapchain_.reset(); }

std::optional<vkrndr::image_t> ngnwsi::render_window_t::acquire_next_image()
{
    if (window_.is_minimized())
    {
        return std::nullopt;
    }

    if (swapchain_->needs_refresh())
    {
        spdlog::info("Recreating swapchain");
        // TODO-JK: Fix global wait
        vkDeviceWaitIdle(device_->logical);
        swapchain_->recreate();
        return std::nullopt;
    }

    if (!swapchain_->acquire_next_image(current_frame_, image_index_))
    {
        return std::nullopt;
    }

    return std::make_optional<vkrndr::image_t>(swapchain_->image(image_index_),
        VK_NULL_HANDLE,
        swapchain_->image_view(image_index_),
        swapchain_->image_format(),
        VK_SAMPLE_COUNT_1_BIT,
        1,
        vkrndr::to_3d_extent(swapchain_->extent()));
}

void ngnwsi::render_window_t::present(
    std::span<VkCommandBuffer const> const& command_buffers)
{
    swapchain_->submit_command_buffers(command_buffers,
        current_frame_,
        image_index_);

    current_frame_ = (current_frame_ + 1) % frames_in_flight_;
}
