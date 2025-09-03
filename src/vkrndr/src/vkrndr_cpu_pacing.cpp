#include <vkrndr_cpu_pacing.hpp>

#include <vkrndr_debug_utils.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_error_code.hpp>
#include <vkrndr_utility.hpp>

#include <fmt/format.h>

#include <vulkan/utility/vk_struct_helper.hpp>

#include <bit>
#include <cstdint>
#include <system_error>
#include <utility>

// IWYU pragma: no_include <boost/move/detail/reverse_iterator.hpp>
// IWYU pragma: no_include <set>
// IWYU pragma: no_include <string>
// IWYU pragma: no_include <variant>

vkrndr::cpu_pacing_t::cpu_pacing_t(device_t const& device,
    uint32_t const frames_in_flight)
    : device_{&device}
    , frames_{frames_in_flight}
    , frames_in_flight_{frames_in_flight}
{
    VkFenceCreateInfo const signaled_fence_create{
        .sType = vku::GetSType<VkFenceCreateInfo>(),
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    uint32_t index{};
    for (frame_in_flight_t& frame : frames_)
    {
        frame.index = index++;

        if (VkResult const result{vkCreateFence(*device_,
                &signaled_fence_create,
                nullptr,
                &frame.submit_fence)};
            !is_success_result(result))
        {
            throw std::system_error{make_error_code(result)};
        }

        VKRNDR_IF_DEBUG_UTILS(object_name(*device_,
            VK_OBJECT_TYPE_FENCE,
            std::bit_cast<uint64_t>(frame.submit_fence),
            fmt::format("CPU pacing fence {}", frame.index)));
    }
}

vkrndr::cpu_pacing_t::~cpu_pacing_t()
{
    for (frame_in_flight_t const& frame : frames_)
    {
        vkDestroyFence(*device_, frame.submit_fence, nullptr);

        for (auto it{frame.cleanup.rbegin()}; it != frame.cleanup.rend(); ++it)
        {
            (*it)();
        }

        for (auto it{frame.old_cleanup_queue.rbegin()};
            it != frame.old_cleanup_queue.rend();
            ++it)
        {
            (*it)();
        }
    }
}

vkrndr::frame_in_flight_t* vkrndr::cpu_pacing_t::pace(uint64_t const timeout)
{
    // NOLINTBEGIN(readability-else-after-return)
    if (VkResult const result{vkWaitForFences(*device_,
            1,
            &frames_[current_frame_].submit_fence,
            VK_TRUE,
            timeout)};
        result == VK_TIMEOUT)
    {
        return nullptr;
    }
    else if (!is_success_result(result))
    {
        throw std::system_error{make_error_code(result)};
    }
    // NOLINTEND(readability-else-after-return)

    auto& frame{current()};
    for (auto it{frame.old_cleanup_queue.rbegin()};
        it != frame.old_cleanup_queue.rend();
        ++it)
    {
        (*it)();
    }
    frame.old_cleanup_queue.clear();

    using std::swap;
    swap(frame.old_cleanup_queue, frame.cleanup);

    return &frame;
}

void vkrndr::cpu_pacing_t::begin_frame()
{
    if (VkResult const result{
            vkResetFences(*device_, 1, &frames_[current_frame_].submit_fence)};
        result != VK_SUCCESS)
    {
        throw std::system_error{make_error_code(result)};
    }
}

void vkrndr::cpu_pacing_t::end_frame()
{
    current_frame_ = (current_frame_ + 1) % frames_in_flight_;
}
