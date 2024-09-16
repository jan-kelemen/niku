#include <vkrndr_execution_port.hpp>

#include <vkrndr_utility.hpp>

#include <volk.h>

vkrndr::execution_port_t::execution_port_t(VkDevice const device,
    VkQueueFlags const queue_flags,
    uint32_t const queue_family,
    uint32_t const queue_index)
    : queue_flags_{queue_flags}
    , queue_family_{queue_family}
{
    vkGetDeviceQueue(device, queue_family, queue_index, &queue_);
}

bool vkrndr::execution_port_t::has_graphics() const
{
    return supports_flags(queue_flags_, VK_QUEUE_GRAPHICS_BIT);
}

bool vkrndr::execution_port_t::has_compute() const
{
    return supports_flags(queue_flags_, VK_QUEUE_COMPUTE_BIT);
}

bool vkrndr::execution_port_t::has_transfer() const
{
    return supports_flags(queue_flags_, VK_QUEUE_TRANSFER_BIT);
}

VkQueue vkrndr::execution_port_t::queue() const { return queue_; }

uint32_t vkrndr::execution_port_t::queue_family() const
{
    return queue_family_;
}

void vkrndr::execution_port_t::submit(
    std::span<VkSubmitInfo const> const submits,
    VkFence const fence)
{
    check_result(vkQueueSubmit(queue_,
        count_cast(submits.size()),
        submits.data(),
        fence));
}

VkResult vkrndr::execution_port_t::present(VkPresentInfoKHR const& present_info)
{
    return vkQueuePresentKHR(queue_, &present_info);
}

void vkrndr::execution_port_t::wait_idle() { vkQueueWaitIdle(queue_); }
