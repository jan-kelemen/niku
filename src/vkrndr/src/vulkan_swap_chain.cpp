#include <vulkan_swap_chain.hpp>

#include <global_data.hpp>
#include <vkrndr_render_settings.hpp>
#include <vulkan_context.hpp>
#include <vulkan_device.hpp>
#include <vulkan_image.hpp>
#include <vulkan_queue.hpp>
#include <vulkan_synchronization.hpp>
#include <vulkan_utility.hpp>
#include <vulkan_window.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <ranges>
#include <span>

// IWYU pragma: no_include <functional>

namespace
{
    [[nodiscard]] VkSurfaceFormatKHR choose_swap_surface_format(
        std::span<VkSurfaceFormatKHR const> available_formats,
        vkrndr::render_settings const& settings)
    {
        if (auto const it{std::ranges::find_if(available_formats,
                [&settings](VkSurfaceFormatKHR const& format)
                {
                    return format.format ==
                        settings.preferred_swapchain_format &&
                        format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
                })};
            it != available_formats.end())
        {
            return *it;
        }

        return available_formats.front();
    }

    [[nodiscard]] VkPresentModeKHR choose_swap_present_mode(
        std::span<VkPresentModeKHR const> available_present_modes,
        vkrndr::render_settings const& settings)
    {
        return std::ranges::find(available_present_modes,
                   settings.preferred_present_mode) !=
                available_present_modes.end()
            ? settings.preferred_present_mode
            : VK_PRESENT_MODE_FIFO_KHR;
    }
} // namespace

void vkrndr::detail::destroy(vulkan_device const* const device,
    swap_frame* const frame)
{
    vkDestroyImageView(device->logical, frame->image_view, nullptr);
    vkDestroySemaphore(device->logical, frame->image_available, nullptr);
    vkDestroySemaphore(device->logical, frame->render_finished, nullptr);
    vkDestroyFence(device->logical, frame->in_flight, nullptr);
}

vkrndr::swap_chain_support
vkrndr::query_swap_chain_support(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    vkrndr::swap_chain_support rv;

    check_result(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device,
        surface,
        &rv.capabilities));

    uint32_t format_count{};
    check_result(vkGetPhysicalDeviceSurfaceFormatsKHR(device,
        surface,
        &format_count,
        nullptr));
    if (format_count != 0)
    {
        rv.surface_formats.resize(format_count);
        check_result(vkGetPhysicalDeviceSurfaceFormatsKHR(device,
            surface,
            &format_count,
            rv.surface_formats.data()));
    }

    uint32_t present_count{};
    check_result(vkGetPhysicalDeviceSurfacePresentModesKHR(device,
        surface,
        &present_count,
        nullptr));
    if (present_count != 0)
    {
        rv.present_modes.resize(present_count);
        check_result(vkGetPhysicalDeviceSurfacePresentModesKHR(device,
            surface,
            &present_count,
            rv.present_modes.data()));
    }

    return rv;
}

vkrndr::vulkan_swap_chain::vulkan_swap_chain(vulkan_window* const window,
    vulkan_context* const context,
    vulkan_device* const device,
    render_settings const* const settings)
    : window_{window}
    , context_{context}
    , device_{device}
    , settings_{settings}
    , present_queue_{device->present_queue}
{
    create_swap_frames();
}

vkrndr::vulkan_swap_chain::~vulkan_swap_chain() { cleanup(); }

bool vkrndr::vulkan_swap_chain::acquire_next_image(size_t const current_frame,
    uint32_t& image_index)
{
    constexpr auto timeout{std::numeric_limits<uint64_t>::max()};

    auto const& frame{frames_[current_frame]};

    check_result(vkWaitForFences(device_->logical,
        1,
        &frame.in_flight,
        VK_TRUE,
        timeout));

    VkResult const result{vkAcquireNextImageKHR(device_->logical,
        chain_,
        timeout,
        frame.image_available,
        VK_NULL_HANDLE,
        &image_index)};
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        swap_chain_refresh.store(true);
        return false;
    }
    check_result(result);

    check_result(vkResetFences(device_->logical, 1, &frame.in_flight));
    return true;
}

void vkrndr::vulkan_swap_chain::submit_command_buffers(
    std::span<VkCommandBuffer const> command_buffers,
    size_t const current_frame,
    uint32_t const image_index)
{
    auto const& frame{frames_[current_frame]};

    auto const* const wait_semaphores{&frame.image_available};
    std::array<VkPipelineStageFlags, 1> const wait_stages{
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    auto const* const signal_semaphores{&frame.render_finished};

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages.data();
    submit_info.commandBufferCount = count_cast(command_buffers.size());
    submit_info.pCommandBuffers = command_buffers.data();
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    check_result(
        vkQueueSubmit(present_queue_->queue, 1, &submit_info, frame.in_flight));

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &chain_;
    present_info.pImageIndices = &image_index;

    VkResult const result{
        vkQueuePresentKHR(present_queue_->queue, &present_info)};
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        swap_chain_refresh.store(true);
        return;
    }
    check_result(result);
}

void vkrndr::vulkan_swap_chain::recreate()
{
    cleanup();
    create_swap_frames();
}

void vkrndr::vulkan_swap_chain::create_swap_frames()
{
    auto swap_details{
        query_swap_chain_support(device_->physical, context_->surface)};

    VkPresentModeKHR const present_mode{
        choose_swap_present_mode(swap_details.present_modes, *settings_)};
    VkSurfaceFormatKHR const surface_format{
        choose_swap_surface_format(swap_details.surface_formats, *settings_)};

    image_format_ = surface_format.format;
    extent_ = window_->swap_extent(swap_details.capabilities);
    min_image_count_ = swap_details.capabilities.minImageCount;

    uint32_t used_image_count{min_image_count_ + 1};
    if (swap_details.capabilities.maxImageCount > 0)
    {
        used_image_count =
            std::min(swap_details.capabilities.maxImageCount, used_image_count);
    }

    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = context_->surface;
    create_info.minImageCount = used_image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent_;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage =
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    create_info.preTransform = swap_details.capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.queueFamilyIndexCount = 0;
    create_info.pQueueFamilyIndices = nullptr;

    check_result(
        vkCreateSwapchainKHR(device_->logical, &create_info, nullptr, &chain_));

    check_result(vkGetSwapchainImagesKHR(device_->logical,
        chain_,
        &used_image_count,
        nullptr));

    std::vector<VkImage> images{used_image_count};
    check_result(vkGetSwapchainImagesKHR(device_->logical,
        chain_,
        &used_image_count,
        images.data()));

    frames_.resize(used_image_count);
    for (size_t i{}; i != used_image_count; ++i)
    {
        auto& frame{frames_[i]};

        frame.image = images[i];
        frame.image_view = create_image_view(*device_,
            images[i],
            image_format_,
            VK_IMAGE_ASPECT_COLOR_BIT,
            1);
        frame.image_available = create_semaphore(device_);
        frame.render_finished = create_semaphore(device_);
        frame.in_flight = create_fence(device_, true);
    }
}

void vkrndr::vulkan_swap_chain::cleanup()
{
    for (detail::swap_frame& frame : frames_)
    {
        destroy(device_, &frame);
    }

    vkDestroySwapchainKHR(device_->logical, chain_, nullptr);
}
