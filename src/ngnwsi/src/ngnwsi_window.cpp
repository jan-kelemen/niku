#include "ngnwsi_sdl_window.hpp"
#include <ngnwsi_window.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_swap_chain.hpp>

#include <spdlog/spdlog.h>

ngnwsi::window_t::window_t(char const* title,
    SDL_WindowFlags window_flags,
    int width,
    int height)
    : window_{title, window_flags, width, height}
{
}

ngnwsi::window_t::~window_t()
{
    swapchain_.reset();

    if (window_.surface())
    {
        window_.destroy_surface(instance_);
    }
}

uint32_t ngnwsi::window_t::id() const { return window_.id(); }

bool ngnwsi::window_t::is_focused() const { return window_.is_focused(); }

VkFormat ngnwsi::window_t::image_format() const
{
    return swapchain_->image_format();
}

uint32_t ngnwsi::window_t::image_count() const
{
    return swapchain_->image_count();
}

VkExtent2D ngnwsi::window_t::extent() const { return swapchain_->extent(); }

vkrndr::image_t ngnwsi::window_t::swapchain_image() const
{
    return {.image = swapchain_->image(image_index_),
        .allocation = VK_NULL_HANDLE,
        .view = swapchain_->image_view(image_index_),
        .format = swapchain_->image_format(),
        .sample_count = VK_SAMPLE_COUNT_1_BIT,
        .mip_levels = 1,
        .extent = vkrndr::to_3d_extent(swapchain_->extent())};
}

ngnwsi::sdl_window_t const& ngnwsi::window_t::native_window() const
{
    return window_;
}

VkSurfaceKHR ngnwsi::window_t::create_surface(VkInstance instance)
{
    instance_ = instance;

    return window_.create_surface(instance);
}

vkrndr::swap_chain_t* ngnwsi::window_t::create_swapchain(
    vkrndr::device_t& device,
    vkrndr::render_settings_t const& settings)
{
    swapchain_ =
        std::make_unique<vkrndr::swap_chain_t>(window_, device, settings);

    device_ = &device;

    frames_in_flight_ = settings.frames_in_flight;

    return swapchain_.get();
}

bool ngnwsi::window_t::acquire_image()
{
    if (window_.is_minimized())
    {
        return false;
    }

    if (needs_refresh_)
    {
        spdlog::info("Recreating swapchain");
        vkDeviceWaitIdle(device_->logical);
        swapchain_->recreate();
        needs_refresh_ = false;
    }

    if (!swapchain_->acquire_next_image(frame_index_,
            image_index_,
            needs_refresh_))
    {
        return false;
    }

    return true;
}

void ngnwsi::window_t::present(
    std::span<VkCommandBuffer const> const& command_buffers)
{
    swapchain_->submit_command_buffers(command_buffers,
        frame_index_,
        image_index_,
        needs_refresh_);

    frame_index_ = (frame_index_ + 1) % frames_in_flight_;
}
