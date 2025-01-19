#include <vkrndr_swap_chain.hpp>

#include <vkrndr_device.hpp>
#include <vkrndr_execution_port.hpp>
#include <vkrndr_global_data.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_render_settings.hpp>
#include <vkrndr_synchronization.hpp>
#include <vkrndr_utility.hpp>
#include <vkrndr_window.hpp>

#include <cppext_container.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

// IWYU pragma: no_include <functional>
// IWYU pragma: no_include <utility>

namespace
{
    [[nodiscard]] VkSurfaceFormatKHR choose_swap_surface_format(
        std::span<VkSurfaceFormatKHR const> available_formats,
        vkrndr::render_settings_t const& settings)
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
        vkrndr::render_settings_t const& settings)
    {
        return std::ranges::find(available_present_modes,
                   settings.preferred_present_mode) !=
                available_present_modes.end()
            ? settings.preferred_present_mode
            : VK_PRESENT_MODE_FIFO_KHR;
    }
} // namespace

void vkrndr::detail::destroy(device_t const* const device,
    swap_frame_t* const frame)
{
    vkDestroySemaphore(device->logical, frame->image_available, nullptr);
    vkDestroySemaphore(device->logical, frame->render_finished, nullptr);
    vkDestroyFence(device->logical, frame->in_flight, nullptr);
}

vkrndr::swap_chain_support_t
vkrndr::query_swap_chain_support(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    vkrndr::swap_chain_support_t rv;

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

vkrndr::swap_chain_t::swap_chain_t(window_t const& window,
    device_t& device,
    render_settings_t const& settings)
    : window_{&window}
    , device_{&device}
    , settings_{&settings}
    , present_queue_{device.present_queue}
{
    create_swap_frames();
}

vkrndr::swap_chain_t::~swap_chain_t() { cleanup(); }

bool vkrndr::swap_chain_t::acquire_next_image(size_t const current_frame,
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

void vkrndr::swap_chain_t::submit_command_buffers(
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

    present_queue_->submit(cppext::as_span(submit_info), frame.in_flight);

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &chain_;
    present_info.pImageIndices = &image_index;

    VkResult const result{present_queue_->present(present_info)};
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        swap_chain_refresh.store(true);
        return;
    }
    check_result(result);
}

void vkrndr::swap_chain_t::recreate()
{
    cleanup();
    create_swap_frames();
}

void vkrndr::swap_chain_t::create_swap_frames()
{
    auto swap_details{
        query_swap_chain_support(device_->physical, window_->surface())};

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
    create_info.surface = window_->surface();
    create_info.minImageCount = used_image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent_;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = settings_->swapchain_flags;
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

    std::vector<VkImage> swapchain_images{used_image_count};
    check_result(vkGetSwapchainImagesKHR(device_->logical,
        chain_,
        &used_image_count,
        swapchain_images.data()));
    images_.reserve(swapchain_images.size());
    // cppcheck-suppress-begin useStlAlgorithm
    for (VkImage swapchain_image : swapchain_images)
    {
        images_.emplace_back(swapchain_image,
            VK_NULL_HANDLE,
            create_image_view(*device_,
                swapchain_image,
                image_format_,
                VK_IMAGE_ASPECT_COLOR_BIT,
                1),
            image_format_,
            VK_SAMPLE_COUNT_1_BIT,
            1,
            to_3d_extent(extent_));
    }
    // cppcheck-suppress-end useStlAlgorithm

    frames_.resize(settings_->frames_in_flight);
    for (detail::swap_frame_t& frame : frames_)
    {
        frame.image_available = create_semaphore(*device_);
        frame.render_finished = create_semaphore(*device_);
        frame.in_flight = create_fence(*device_, true);
    }
}

void vkrndr::swap_chain_t::cleanup()
{
    for (detail::swap_frame_t& frame : frames_)
    {
        destroy(device_, &frame);
    }
    frames_.clear();

    for (vkrndr::image_t const& swapchain_image : images_)
    {
        vkDestroyImageView(device_->logical, swapchain_image.view, nullptr);
    }
    images_.clear();

    vkDestroySwapchainKHR(device_->logical, chain_, nullptr);
}
