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
#include <cppext_pragma_warning.hpp>

#include <vulkan/utility/vk_struct_helper.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <ranges>
#include <span>

// IWYU pragma: no_include <boost/container/vector.hpp>
// IWYU pragma: no_include <boost/intrusive/detail/iterator.hpp>
// IWYU pragma: no_include <fmt/base.h>
// IWYU pragma: no_include <fmt/format.h>
// IWYU pragma: no_include <expected>
// IWYU pragma: no_include <initializer_list>
// IWYU pragma: no_include <functional>
// IWYU pragma: no_include <set>
// IWYU pragma: no_include <string>
// IWYU pragma: no_include <system_error>
// IWYU pragma: no_include <utility>

namespace
{
    static_assert(VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_KHR ==
        VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_EXT);

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

    [[nodiscard]] VkCompositeAlphaFlagBitsKHR choose_composite_alpha(
        VkSurfaceCapabilitiesKHR const& capabilities)
    {
        for (VkCompositeAlphaFlagBitsKHR const flag :
            {VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
                VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
                VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR})
        {
            DISABLE_WARNING_PUSH
            DISABLE_WARNING_SIGN_CONVERSION
            if (static_cast<VkCompositeAlphaFlagBitsKHR>(
                    capabilities.supportedCompositeAlpha & flag) == flag)
            {
                return flag;
            }
            DISABLE_WARNING_POP
        }

        assert(false);
        return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    }

    [[nodiscard]] bool is_present_mode_available(vkrndr::device_t const& device,
        VkPresentModeKHR const mode)
    {
        if (mode == VK_PRESENT_MODE_FIFO_LATEST_READY_KHR)
        {
            static_assert(VK_PRESENT_MODE_FIFO_LATEST_READY_KHR ==
                VK_PRESENT_MODE_FIFO_LATEST_READY_EXT);

            return device.extensions.contains(
                       VK_KHR_PRESENT_MODE_FIFO_LATEST_READY_EXTENSION_NAME) ||
                device.extensions.contains(
                    VK_KHR_PRESENT_MODE_FIFO_LATEST_READY_EXTENSION_NAME);
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
        VkSurfacePresentModeCompatibilityKHR compatibility{
            .sType = vku::GetSType<VkSurfacePresentModeCompatibilityKHR>(),
            .presentModeCount = vkrndr::count_cast(results),
            .pPresentModes = results.data()};

        VkSurfacePresentModeKHR surface_present_mode{
            .sType = vku::GetSType<VkSurfacePresentModeKHR>(),
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
                                           VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME,
                                           device) ||
          is_device_extension_enabled(
              VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME,
              device)}
    , fence_pool_{device}
    , semaphore_pool_{device}
{
    create_swap_frames(false, 0);
}

vkrndr::swapchain_t::~swapchain_t()
{
    for (detail::swap_frame_t& frame : frames_in_flight_)
    {
        if (VkResult const result{
                semaphore_pool_.recycle(frame.acquire_semaphore)};
            result != VK_SUCCESS)
        {
            spdlog::error("Semaphore recycle failed: {}",
                make_error_code(result).message());
        }

        cleanup_images(frame.garbage);

        assert(frame.present_semaphore == VK_NULL_HANDLE);
    }

    try
    {
        for (detail::present_operation_t& present_info : present_history_)
        {
            if (present_info.cleanup_fence != VK_NULL_HANDLE)
            {
                vkWaitForFences(*device_,
                    1,
                    &present_info.cleanup_fence,
                    VK_TRUE,
                    UINT64_MAX);
            }

            cleanup_present_info(present_info);
        }

        std::ranges::for_each(old_swapchains_,
            [this](detail::cleanup_data_t& data) { cleanup_swapchain(data); });
    }
    catch (std::system_error const&)
    {
        assert(false);
    }

    cleanup_images(images_);

    vkDestroySwapchainKHR(*device_, handle_, nullptr);
}

std::optional<vkrndr::image_t> vkrndr::swapchain_t::acquire_next_image(
    size_t const current_frame)
{
    auto& frame{frames_in_flight_[current_frame]};
    if (frame.acquire_semaphore != VK_NULL_HANDLE)
    {
        if (VkResult const result{
                semaphore_pool_.recycle(frame.acquire_semaphore)};
            result != VK_SUCCESS)
        {
            throw std::system_error{make_error_code(result)};
        }
        frame.acquire_semaphore = VK_NULL_HANDLE;

        cleanup_images(frame.garbage);

        assert(frame.present_semaphore == VK_NULL_HANDLE);
    }

    VkFence const acquire_fence{swapchain_maintenance_1_enabled_
            ? VK_NULL_HANDLE
            : fence_pool_.get().value()};
    VkSemaphore const acquire_semaphore{semaphore_pool_.get().value()};
    VkSemaphore const present_semaphore{semaphore_pool_.get().value()};

    VkResult const acquire_result{vkAcquireNextImageKHR(*device_,
        handle_,
        0,
        acquire_semaphore,
        acquire_fence,
        &frame.image_index)};
    if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR ||
        acquire_result == VK_SUBOPTIMAL_KHR)
    {
        swapchain_refresh_needed_ = true;
    }

    if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR ||
        acquire_result == VK_NOT_READY ||
        acquire_result == VK_TIMEOUT)
    {
        if (acquire_fence != VK_NULL_HANDLE)
        {
            if (VkResult const result{fence_pool_.recycle(acquire_fence)};
                result != VK_SUCCESS)
            {
                throw std::system_error{make_error_code(result)};
            }
        }

        if (VkResult const result{semaphore_pool_.recycle(acquire_semaphore)};
            result != VK_SUCCESS)
        {
            throw std::system_error{make_error_code(result)};
        }

        if (VkResult const result{semaphore_pool_.recycle(present_semaphore)};
            result != VK_SUCCESS)
        {
            throw std::system_error{make_error_code(result)};
        }

        return std::nullopt;
    }
    check_result(acquire_result);

    if (swapchain_maintenance_1_enabled_)
    {
        if (detail::swap_image_t& image{images_[frame.image_index]};
            image.view == VK_NULL_HANDLE)
        {
            image.view = create_image_view(*device_,
                image.handle,
                image_format_,
                VK_IMAGE_ASPECT_COLOR_BIT,
                1);
        }
    }
    else
    {
        associate_with_present_history(frame.image_index, acquire_fence);
    }

    frame.acquire_semaphore = acquire_semaphore;
    frame.present_semaphore = present_semaphore;

    return std::make_optional<vkrndr::image_t>(
        images_[frame.image_index].handle,
        VK_NULL_HANDLE,
        images_[frame.image_index].view,
        image_format_,
        VK_SAMPLE_COUNT_1_BIT,
        1,
        vkrndr::to_3d_extent(extent_));
}

void vkrndr::swapchain_t::submit_command_buffers(
    std::span<VkCommandBuffer const> command_buffers,
    size_t const current_frame,
    VkFence const submit_fence)
{
    auto& frame{frames_in_flight_[current_frame]};

    VkPipelineStageFlags const wait_stage{
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSubmitInfo const submit_info{.sType = vku::GetSType<VkSubmitInfo>(),
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &frame.acquire_semaphore,
        .pWaitDstStageMask = &wait_stage,
        .commandBufferCount = count_cast(command_buffers),
        .pCommandBuffers = command_buffers.data(),
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &frame.present_semaphore};

    settings_.present_queue->submit(cppext::as_span(submit_info), submit_fence);

    VkPresentInfoKHR present_info{.sType = vku::GetSType<VkPresentInfoKHR>(),
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &frame.present_semaphore,
        .swapchainCount = 1,
        .pSwapchains = &handle_,
        .pImageIndices = &frame.image_index};

    VkFence present_fence{VK_NULL_HANDLE};
    VkSwapchainPresentFenceInfoKHR fence_info{
        .sType = vku::GetSType<VkSwapchainPresentFenceInfoKHR>(),
        .swapchainCount = 1,
        .pFences = &present_fence};
    if (swapchain_maintenance_1_enabled_)
    {
        present_fence = fence_pool_.get().value();
        fence_info.pNext = std::exchange(present_info.pNext, &fence_info);
    }

    VkSwapchainPresentModeInfoKHR present_mode_info{
        .sType = vku::GetSType<VkSwapchainPresentModeInfoKHR>(),
        .swapchainCount = 1,
        .pPresentModes = &desired_present_mode_};
    if (desired_present_mode_ != current_present_mode_)
    {
        if (swapchain_maintenance_1_enabled_ &&
            std::ranges::contains(compatible_present_modes_,
                desired_present_mode_))
        {
            present_mode_info.pNext =
                std::exchange(present_info.pNext, &present_mode_info);
            current_present_mode_ = desired_present_mode_;
        }
        else
        {
            swapchain_refresh_needed_ = true;
        }
    }

    VkResult const result{settings_.present_queue->present(present_info)};
    swapchain_refresh_needed_ =
        result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR;

    add_to_present_history(frame.image_index, present_fence, frame);
    cleanup_present_history();

    if (result != VK_ERROR_OUT_OF_DATE_KHR)
    {
        check_result(result);
    }
}

void vkrndr::swapchain_t::recreate(uint32_t const current_frame)
{
    create_swap_frames(true, current_frame);
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

void vkrndr::swapchain_t::create_swap_frames(bool const is_recreated,
    uint32_t const current_frame)
{
    swapchain_support_t const swap_details{
        query_swapchain_support(*device_, window_->surface())};

    VkPresentModeKHR const chosen_present_mode{
        choose_swap_present_mode(swap_details.present_modes,
            is_recreated ? desired_present_mode_
                         : settings_.preferred_present_mode)};
    VkSurfaceFormatKHR const surface_format{
        choose_swap_surface_format(swap_details.surface_formats, settings_)};

    available_present_modes_.clear();

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

    if (swap_details.capabilities.currentExtent.width !=
        std::numeric_limits<uint32_t>::max())
    {
        extent_ = swap_details.capabilities.currentExtent;
    }
    else
    {
        VkExtent2D const window_extent{window_->swap_extent()};
        extent_.width = std::clamp(window_extent.width,
            swap_details.capabilities.minImageExtent.width,
            swap_details.capabilities.maxImageExtent.width);
        extent_.height = std::clamp(window_extent.height,
            swap_details.capabilities.minImageExtent.height,
            swap_details.capabilities.maxImageExtent.height);
    }

    min_image_count_ = swap_details.capabilities.minImageCount;

    current_present_mode_ = chosen_present_mode;
    desired_present_mode_ = chosen_present_mode;

    uint32_t used_image_count{min_image_count_ + 1};
    if (swap_details.capabilities.maxImageCount > 0)
    {
        used_image_count =
            std::min(swap_details.capabilities.maxImageCount, used_image_count);
    }

    VkSwapchainKHR const old_swapchain{handle_};

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
        .compositeAlpha = choose_composite_alpha(swap_details.capabilities),
        .presentMode = chosen_present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = old_swapchain,
    };

    VkSwapchainPresentModesCreateInfoKHR const present_modes{
        .sType = vku::GetSType<VkSwapchainPresentModesCreateInfoKHR>(),
        .presentModeCount = count_cast(compatible_present_modes_),
        .pPresentModes = compatible_present_modes_.data()};

    if (swapchain_maintenance_1_enabled_)
    {
        create_info.flags =
            VK_SWAPCHAIN_CREATE_DEFERRED_MEMORY_ALLOCATION_BIT_KHR;
        create_info.pNext = &present_modes;
    }

    check_result(
        vkCreateSwapchainKHR(*device_, &create_info, nullptr, &handle_));

    frames_in_flight_.resize(settings_.frames_in_flight);

    std::ranges::move(images_,
        std::back_inserter(frames_in_flight_[current_frame].garbage));
    images_.clear();

    if (old_swapchain != VK_NULL_HANDLE)
    {
        schedule_destruction(old_swapchain);
    }

    check_result(
        vkGetSwapchainImagesKHR(*device_, handle_, &used_image_count, nullptr));

    std::vector<VkImage> swapchain_images{used_image_count};
    check_result(vkGetSwapchainImagesKHR(*device_,
        handle_,
        &used_image_count,
        swapchain_images.data()));
    images_.reserve(swapchain_images.size());
    // cppcheck-suppress-begin useStlAlgorithm

    for (VkImage const image : swapchain_images)
    {
        images_.emplace_back(image,
            swapchain_maintenance_1_enabled_ ? VK_NULL_HANDLE
                                             : create_image_view(*device_,
                                                   image,
                                                   image_format_,
                                                   VK_IMAGE_ASPECT_COLOR_BIT,
                                                   1));
    }

    // cppcheck-suppress-end useStlAlgorithm

    swapchain_refresh_needed_ = false;
}

void vkrndr::swapchain_t::cleanup_images(
    std::vector<detail::swap_image_t>& images)
{
    for (detail::swap_image_t const& image : images)
    {
        vkDestroyImageView(*device_, image.view, nullptr);
    }
    images.clear();
}

void vkrndr::swapchain_t::schedule_destruction(VkSwapchainKHR const swapchain)
{
    // If no presentation is done on the swapchain, destroy it right away.
    if (!present_history_.empty() &&
        present_history_.back().image_index == detail::invalid_image_index)
    {
        vkDestroySwapchainKHR(*device_, swapchain, nullptr);
        return;
    }

    detail::cleanup_data_t cleanup{.swapchain = swapchain};

    std::vector<detail::present_operation_t> history_to_keep;
    while (!present_history_.empty())
    {
        detail::present_operation_t& present_info{present_history_.back()};

        if (present_info.image_index == detail::invalid_image_index)
        {
            assert(present_info.cleanup_fence != VK_NULL_HANDLE);
            break;
        }

        present_info.image_index = detail::invalid_image_index;

        if (present_info.cleanup_fence != VK_NULL_HANDLE)
        {
            history_to_keep.push_back(std::move(present_info));
        }
        else
        {
            assert(present_info.present_semaphore != VK_NULL_HANDLE);

            cleanup.semaphores.push_back(present_info.present_semaphore);

            std::ranges::move(present_info.old_swapchains,
                std::back_inserter(old_swapchains_));

            present_info.old_swapchains.clear();
        }

        present_history_.pop_back();
    }

    std::ranges::move(history_to_keep, std::back_inserter(present_history_));

    if (cleanup.swapchain != VK_NULL_HANDLE || !cleanup.semaphores.empty())
    {
        old_swapchains_.emplace_back(std::move(cleanup));
    }
}

void vkrndr::swapchain_t::associate_with_present_history(
    uint32_t const image_index,
    VkFence const fence)
{
    for (detail::present_operation_t& present_operation :
        std::views::reverse(present_history_))
    {
        if (present_operation.image_index == detail::invalid_image_index)
        {
            break;
        }

        if (present_operation.image_index == image_index)
        {
            assert(present_operation.cleanup_fence == VK_NULL_HANDLE);
            present_operation.cleanup_fence = fence;
            return;
        }
    }

    detail::present_operation_t& present_operation{
        present_history_.emplace_back()};
    present_operation.cleanup_fence = fence;
    present_operation.image_index = image_index;
}

void vkrndr::swapchain_t::add_to_present_history(uint32_t const image_index,
    VkFence const present_fence,
    detail::swap_frame_t& frame)
{
    detail::present_operation_t& present_operation{
        present_history_.emplace_back()};

    present_operation.present_semaphore =
        std::exchange(frame.present_semaphore, VK_NULL_HANDLE);

    present_operation.old_swapchains = std::move(old_swapchains_);
    old_swapchains_ = {};

    if (swapchain_maintenance_1_enabled_)
    {
        present_operation.cleanup_fence = present_fence;
    }
    else
    {
        present_operation.image_index = image_index;
    }
}

void vkrndr::swapchain_t::cleanup_present_history()
{
    while (!present_history_.empty())
    {
        detail::present_operation_t& operation{present_history_.front()};

        if (operation.cleanup_fence == VK_NULL_HANDLE)
        {
            assert(operation.image_index != detail::invalid_image_index);
            break;
        }

        VkResult const result{
            vkGetFenceStatus(*device_, operation.cleanup_fence)};
        if (result == VK_NOT_READY)
        {
            break;
        }
        check_result(result);

        cleanup_present_info(operation);
        present_history_.pop_front();
    }

    if (present_history_.size() > images_.size() * 2 &&
        present_history_.front().cleanup_fence == VK_NULL_HANDLE)
    {
        detail::present_operation_t present_info{
            std::move(present_history_.front())};
        present_history_.pop_front();

        assert(present_info.image_index != detail::invalid_image_index);

        assert(std::ranges::all_of(present_history_,
            [](detail::present_operation_t const& op)
            { return op.old_swapchains.empty(); }));
        present_history_.front().old_swapchains =
            std::move(present_info.old_swapchains);

        present_history_.push_back(std::move(present_info));
    }
}

void vkrndr::swapchain_t::cleanup_present_info(
    detail::present_operation_t& operation)
{
    if (operation.cleanup_fence != VK_NULL_HANDLE)
    {
        if (VkResult const result{fence_pool_.recycle(operation.cleanup_fence)};
            result != VK_SUCCESS)
        {
            throw std::system_error{make_error_code(result)};
        }
        operation.cleanup_fence = VK_NULL_HANDLE;
    }

    if (operation.present_semaphore != VK_NULL_HANDLE)
    {
        if (VkResult const result{
                semaphore_pool_.recycle(operation.present_semaphore)};
            result != VK_SUCCESS)
        {
            throw std::system_error{make_error_code(result)};
        }
        operation.present_semaphore = VK_NULL_HANDLE;
    }

    std::ranges::for_each(operation.old_swapchains,
        [this](detail::cleanup_data_t& data) { cleanup_swapchain(data); });
    operation.old_swapchains.clear();

    operation.image_index = detail::invalid_image_index;
}

void vkrndr::swapchain_t::cleanup_swapchain(detail::cleanup_data_t& data)
{
    if (data.swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(*device_, data.swapchain, nullptr);
        data.swapchain = VK_NULL_HANDLE;
    }

    for (VkSemaphore const semaphore : data.semaphores)
    {
        if (VkResult const result{semaphore_pool_.recycle(semaphore)};
            result != VK_SUCCESS)
        {
            throw std::system_error{make_error_code(result)};
        }
    }

    data.semaphores.clear();
}
