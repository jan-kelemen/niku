#include <vkrndr_swapchain.hpp>

#include <vkrndr_device.hpp>
#include <vkrndr_error_code.hpp>
#include <vkrndr_execution_port.hpp>
#include <vkrndr_features.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_synchronization.hpp>
#include <vkrndr_utility.hpp>
#include <vkrndr_window.hpp>

#include <cppext_container.hpp>

#include <vulkan/utility/vk_struct_helper.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <ranges>
#include <span>

// IWYU pragma: no_include <functional>
// IWYU pragma: no_include <set>
// IWYU pragma: no_include <string>
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
                    return format.format == settings.preferred_format &&
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

    [[nodiscard]] bool is_present_mode_available(vkrndr::device_t const& device,
        VkPresentModeKHR const mode)
    {
        if (mode == VK_PRESENT_MODE_FIFO_LATEST_READY_EXT)
        {
            return device.extensions.contains(
                VK_EXT_PRESENT_MODE_FIFO_LATEST_READY_EXTENSION_NAME);
        }

        if (mode == VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR ||
            mode == VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR)
        {
            return device.extensions.contains(
                VK_KHR_SHARED_PRESENTABLE_IMAGE_EXTENSION_NAME);
        }

        return mode >= VK_PRESENT_MODE_IMMEDIATE_KHR &&
            mode <= VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    }

    [[nodiscard]] std::vector<VkPresentModeKHR> query_compatible_present_modes(
        vkrndr::device_t const& device,
        VkSurfaceKHR const surface,
        VkPresentModeKHR const present_mode)
    {
        std::array<VkPresentModeKHR, 7> results{};
        VkSurfacePresentModeCompatibilityEXT compatibility{
            .sType = vku::GetSType<VkSurfacePresentModeCompatibilityEXT>(),
            .presentModeCount = vkrndr::count_cast(results.size()),
            .pPresentModes = results.data()};

        VkSurfacePresentModeEXT surface_present_mode{
            .sType = vku::GetSType<VkSurfacePresentModeEXT>(),
            .presentMode = present_mode};

        VkPhysicalDeviceSurfaceInfo2KHR const surface_info{
            .sType = vku::GetSType<VkPhysicalDeviceSurfaceInfo2KHR>(),
            .pNext = &surface_present_mode,
            .surface = surface};

        VkSurfaceCapabilities2KHR surface_capabilities{
            .sType = vku::GetSType<VkSurfaceCapabilities2KHR>(),
            .pNext = &compatibility};

        vkGetPhysicalDeviceSurfaceCapabilities2KHR(device,
            &surface_info,
            &surface_capabilities);

        std::vector<VkPresentModeKHR> rv;
        std::ranges::copy_if(
            results | std::views::take(compatibility.presentModeCount),
            std::back_inserter(rv),
            [&device](VkPresentModeKHR const mode)
            { return is_present_mode_available(device, mode); });

        return rv;
    }
} // namespace

vkrndr::swapchain_t::swapchain_t(window_t const& window,
    device_t& device,
    swapchain_settings_t const& settings)
    : window_{&window}
    , device_{&device}
    , settings_{settings}
    , swapchain_maintenance_1_enabled_{is_device_extension_enabled(
          VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME,
          device)}
    , fence_pool_{device}
    , semaphore_pool_{device}
{
    create_swap_frames(false);
}

vkrndr::swapchain_t::~swapchain_t() { cleanup(); }

bool vkrndr::swapchain_t::acquire_next_image(size_t const current_frame,
    uint32_t& image_index)
{
    auto const& frame{frames_[current_frame]};

    VkResult const wait_result{
        vkWaitForFences(*device_, 1, &frame.in_flight, VK_TRUE, 0)};
    if (wait_result == VK_TIMEOUT)
    {
        return false;
    }
    check_result(wait_result);

    VkResult const acquire_result{vkAcquireNextImageKHR(*device_,
        handle_,
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
    else if (acquire_result == VK_NOT_READY || acquire_result == VK_TIMEOUT)
    {
        return false;
    }

    check_result(acquire_result);

    check_result(vkResetFences(*device_, 1, &frame.in_flight));
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

    VkSubmitInfo submit_info{.sType = vku::GetSType<VkSubmitInfo>(),
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = wait_semaphores,
        .pWaitDstStageMask = wait_stages.data(),
        .commandBufferCount = count_cast(command_buffers.size()),
        .pCommandBuffers = command_buffers.data(),
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = signal_semaphores};

    settings_.present_queue->submit(cppext::as_span(submit_info),
        frame.in_flight);

    VkPresentInfoKHR present_info{.sType = vku::GetSType<VkPresentInfoKHR>(),
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = signal_semaphores,
        .swapchainCount = 1,
        .pSwapchains = &handle_,
        .pImageIndices = &image_index};

    VkSwapchainPresentModeInfoEXT const present_mode_info{
        .sType = vku::GetSType<VkSwapchainPresentModeInfoEXT>(),
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

    VkResult const result{settings_.present_queue->present(present_info)};
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
    auto swap_details{query_swapchain_support(*device_, window_->surface())};

    VkPresentModeKHR const chosen_present_mode{
        choose_swap_present_mode(swap_details.present_modes,
            is_recreated ? desired_present_mode_
                         : settings_.preferred_present_mode)};
    VkSurfaceFormatKHR const surface_format{
        choose_swap_surface_format(swap_details.surface_formats, settings_)};

    std::ranges::copy_if(swap_details.present_modes,
        std::back_inserter(available_present_modes_),
        [this](VkPresentModeKHR const mode)
        { return is_present_mode_available(*device_, mode); });

    if (swapchain_maintenance_1_enabled_)
    {
        compatible_present_modes_ = query_compatible_present_modes(*device_,
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

    VkSwapchainCreateInfoKHR create_info{
        .sType = vku::GetSType<VkSwapchainCreateInfoKHR>(),
        .surface = window_->surface(),
        .minImageCount = used_image_count,
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = extent_,
        .imageArrayLayers = 1,
        .imageUsage = settings_.image_flags,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .preTransform = swap_details.capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = chosen_present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };

    VkSwapchainPresentModesCreateInfoEXT const present_modes{
        .sType = vku::GetSType<VkSwapchainPresentModesCreateInfoEXT>(),
        .presentModeCount = count_cast(compatible_present_modes_.size()),
        .pPresentModes = compatible_present_modes_.data()};

    if (swapchain_maintenance_1_enabled_)
    {
        create_info.pNext = &present_modes;
    }

    check_result(
        vkCreateSwapchainKHR(*device_, &create_info, nullptr, &handle_));

    check_result(
        vkGetSwapchainImagesKHR(*device_, handle_, &used_image_count, nullptr));

    std::vector<VkImage> swapchain_images{used_image_count};
    check_result(vkGetSwapchainImagesKHR(*device_,
        handle_,
        &used_image_count,
        swapchain_images.data()));
    images_.reserve(swapchain_images.size());
    submit_finished_semaphore_.reserve(swapchain_images.size());
    // cppcheck-suppress-begin useStlAlgorithm
    for (VkImage swapchain_image : swapchain_images)
    {
        images_.emplace_back(swapchain_image,
            create_image_view(*device_,
                swapchain_image,
                image_format_,
                VK_IMAGE_ASPECT_COLOR_BIT,
                1));
        submit_finished_semaphore_.push_back(semaphore_pool_.get().value());
    }
    // cppcheck-suppress-end useStlAlgorithm

    frames_.resize(settings_.frames_in_flight);
    for (detail::swap_frame_t& frame : frames_)
    {
        frame.image_available = semaphore_pool_.get().value();
        frame.in_flight = fence_pool_.get(true).value();
    }

    swapchain_refresh_needed_ = false;
}

void vkrndr::swapchain_t::cleanup()
{
    for (detail::swap_frame_t& frame : frames_)
    {
        if (VkResult const result{
                semaphore_pool_.recycle(frame.image_available)};
            result != VK_SUCCESS)
        {
            throw std::system_error{make_error_code(result)};
        }

        if (VkResult const result{fence_pool_.recycle(frame.in_flight)};
            result != VK_SUCCESS)
        {
            throw std::system_error{make_error_code(result)};
        }
    }
    frames_.clear();

    for (detail::swap_image_t const& image : images_)
    {
        vkDestroyImageView(*device_, image.view, nullptr);
    }
    images_.clear();

    for (VkSemaphore const semaphore : submit_finished_semaphore_)
    {
        if (VkResult const result{semaphore_pool_.recycle(semaphore)};
            result != VK_SUCCESS)
        {
            throw std::system_error{make_error_code(result)};
        }
    }
    submit_finished_semaphore_.clear();

    compatible_present_modes_.clear();
    available_present_modes_.clear();

    vkDestroySwapchainKHR(*device_, handle_, nullptr);
}
