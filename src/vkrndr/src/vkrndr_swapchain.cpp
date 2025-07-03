#include <vkrndr_swapchain.hpp>

#include <vkrndr_device.hpp>
#include <vkrndr_execution_port.hpp>
#include <vkrndr_features.hpp>
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
#include <iterator>
#include <limits>
#include <span>

// IWYU pragma: no_include <functional>
// IWYU pragma: no_include <utility>

namespace
{
    [[nodiscard]] VkSurfaceFormatKHR choose_swap_surface_format(
        std::span<VkSurfaceFormatKHR const> available_formats,
        vkrndr::swapchain_settings_t const& settings)
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
        VkPresentModeKHR const preferred_present_mode)
    {
        return std::ranges::find(available_present_modes,
                   preferred_present_mode) != available_present_modes.end()
            ? preferred_present_mode
            : VK_PRESENT_MODE_FIFO_KHR;
    }

    [[nodiscard]] std::vector<VkPresentModeKHR> query_compatible_present_modes(
        VkPhysicalDevice const device,
        VkSurfaceKHR const surface,
        VkPresentModeKHR const present_mode)
    {
        std::array<VkPresentModeKHR, 7> results{};
        VkSurfacePresentModeCompatibilityEXT compatibility{
            .sType = VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_COMPATIBILITY_EXT,
            .presentModeCount = vkrndr::count_cast(results.size()),
            .pPresentModes = results.data()};

        VkSurfacePresentModeEXT surface_present_mode{
            .sType = VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_EXT,
            .presentMode = present_mode};

        VkPhysicalDeviceSurfaceInfo2KHR const surface_info{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
            .pNext = &surface_present_mode,
            .surface = surface};

        VkSurfaceCapabilities2KHR surface_capabilities{
            .sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR,
            .pNext = &compatibility};

        vkGetPhysicalDeviceSurfaceCapabilities2KHR(device,
            &surface_info,
            &surface_capabilities);

        return {std::cbegin(results),
            std::cbegin(results) + compatibility.presentModeCount};
    }
} // namespace

void vkrndr::detail::destroy(device_t const* const device,
    swap_frame_t* const frame)
{
    vkDestroySemaphore(device->logical, frame->image_available, nullptr);
    vkDestroyFence(device->logical, frame->in_flight, nullptr);
}

vkrndr::swapchain_t::swapchain_t(window_t const& window,
    device_t& device,
    swapchain_settings_t const& settings)
    : window_{&window}
    , device_{&device}
    , settings_{settings}
    , swapchain_maintenance_1_enabled_{is_device_extension_enabled(
          VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME,
          device)}
    , present_queue_{device.present_queue}
{
    create_swap_frames(false);
}

vkrndr::swapchain_t::~swapchain_t() { cleanup(); }

bool vkrndr::swapchain_t::acquire_next_image(size_t const current_frame,
    uint32_t& image_index)
{
    auto const& frame{frames_[current_frame]};

    VkResult const wait_result{
        vkWaitForFences(device_->logical, 1, &frame.in_flight, VK_TRUE, 0)};
    if (wait_result == VK_TIMEOUT)
    {
        return false;
    }
    check_result(wait_result);

    VkResult const acquire_result{vkAcquireNextImageKHR(device_->logical,
        chain_,
        0,
        frame.image_available,
        VK_NULL_HANDLE,
        &image_index)};
    if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        swapchain_refresh_needed_ = true;
        return false;
    }
    if (acquire_result == VK_SUBOPTIMAL_KHR)
    {
        swapchain_refresh_needed_ = true;
    }

    check_result(acquire_result);

    check_result(vkResetFences(device_->logical, 1, &frame.in_flight));
    return true;
}

void vkrndr::swapchain_t::submit_command_buffers(
    std::span<VkCommandBuffer const> command_buffers,
    size_t const current_frame,
    uint32_t const image_index)
{
    auto const& frame{frames_[current_frame]};

    auto const* const wait_semaphores{&frame.image_available};
    std::array<VkPipelineStageFlags, 1> const wait_stages{
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    auto const* const signal_semaphores{
        &submit_finished_semaphore_[image_index]};

    VkSubmitInfo submit_info{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = wait_semaphores,
        .pWaitDstStageMask = wait_stages.data(),
        .commandBufferCount = count_cast(command_buffers.size()),
        .pCommandBuffers = command_buffers.data(),
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = signal_semaphores};

    present_queue_->submit(cppext::as_span(submit_info), frame.in_flight);

    VkPresentInfoKHR present_info{.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = signal_semaphores,
        .swapchainCount = 1,
        .pSwapchains = &chain_,
        .pImageIndices = &image_index};

    VkSwapchainPresentModeInfoEXT const present_mode_info{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODE_INFO_EXT,
        .swapchainCount = 1,
        .pPresentModes = &desired_present_mode_};

    if (desired_present_mode_ != current_present_mode_)
    {
        if (swapchain_maintenance_1_enabled_ &&
            std::ranges::contains(compatible_present_modes_,
                desired_present_mode_))
        {
            present_info.pNext = &present_mode_info;
            current_present_mode_ = desired_present_mode_;
        }
        else
        {
            swapchain_refresh_needed_ = true;
        }
    }

    VkResult const result{present_queue_->present(present_info)};
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        swapchain_refresh_needed_ = true;
        return;
    }
    check_result(result);
}

void vkrndr::swapchain_t::recreate()
{
    cleanup();
    create_swap_frames(true);
}

std::span<VkPresentModeKHR const>
vkrndr::swapchain_t::available_present_modes() const
{
    return available_present_modes_;
}

bool vkrndr::swapchain_t::change_present_mode(VkPresentModeKHR const new_mode)
{
    if (std::ranges::contains(available_present_modes_, new_mode))
    {
        desired_present_mode_ = new_mode;
        return true;
    }

    return false;
}

void vkrndr::swapchain_t::create_swap_frames(bool const is_recreated)
{
    auto swap_details{
        query_swapchain_support(device_->physical, window_->surface())};

    VkPresentModeKHR const chosen_present_mode{
        choose_swap_present_mode(swap_details.present_modes,
            is_recreated ? desired_present_mode_
                         : settings_.preferred_present_mode)};
    VkSurfaceFormatKHR const surface_format{
        choose_swap_surface_format(swap_details.surface_formats, settings_)};

    available_present_modes_ = std::move(swap_details.present_modes);

    if (swapchain_maintenance_1_enabled_)
    {
        compatible_present_modes_ =
            query_compatible_present_modes(device_->physical,
                window_->surface(),
                chosen_present_mode);
    }
    else
    {
        compatible_present_modes_.push_back(chosen_present_mode);
    }

    image_format_ = surface_format.format;
    extent_ = window_->swap_extent(swap_details.capabilities);
    min_image_count_ = swap_details.capabilities.minImageCount;

    current_present_mode_ = chosen_present_mode;
    desired_present_mode_ = chosen_present_mode;

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
    create_info.imageUsage = settings_.swapchain_flags;
    create_info.preTransform = swap_details.capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = chosen_present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.queueFamilyIndexCount = 0;
    create_info.pQueueFamilyIndices = nullptr;

    VkSwapchainPresentModesCreateInfoEXT const present_modes{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODES_CREATE_INFO_EXT,
        .presentModeCount = count_cast(compatible_present_modes_.size()),
        .pPresentModes = compatible_present_modes_.data()};

    if (swapchain_maintenance_1_enabled_)
    {
        create_info.pNext = &present_modes;
    }

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
    submit_finished_semaphore_.reserve(swapchain_images.size());
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
        submit_finished_semaphore_.emplace_back(create_semaphore(*device_));
    }
    // cppcheck-suppress-end useStlAlgorithm

    frames_.resize(settings_.frames_in_flight);
    for (detail::swap_frame_t& frame : frames_)
    {
        frame.image_available = create_semaphore(*device_);
        frame.in_flight = create_fence(*device_, true);
    }

    swapchain_refresh_needed_ = false;
}

void vkrndr::swapchain_t::cleanup()
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

    for (VkSemaphore const semaphore : submit_finished_semaphore_)
    {
        vkDestroySemaphore(device_->logical, semaphore, nullptr);
    }
    submit_finished_semaphore_.clear();

    compatible_present_modes_.clear();
    available_present_modes_.clear();

    vkDestroySwapchainKHR(device_->logical, chain_, nullptr);
}

VkExtent2D vkrndr::swapchain_t::extent() const noexcept { return extent_; }

VkSwapchainKHR vkrndr::swapchain_t::swapchain() const noexcept
{
    return chain_;
}

VkFormat vkrndr::swapchain_t::image_format() const noexcept
{
    return image_format_;
}

uint32_t vkrndr::swapchain_t::min_image_count() const noexcept
{
    return min_image_count_;
}

uint32_t vkrndr::swapchain_t::image_count() const noexcept
{
    return vkrndr::count_cast(frames_.size());
}

VkImage vkrndr::swapchain_t::image(uint32_t const image_index) const noexcept
{
    return images_[image_index].image;
}

VkImageView vkrndr::swapchain_t::image_view(
    uint32_t const image_index) const noexcept
{
    return images_[image_index].view;
}

bool vkrndr::swapchain_t::needs_refresh() const noexcept
{
    return swapchain_refresh_needed_;
}

VkPresentModeKHR vkrndr::swapchain_t::present_mode() const
{
    return current_present_mode_;
}
