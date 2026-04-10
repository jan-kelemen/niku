#include <vkrndr_execution_port.hpp>

#include <vkrndr_utility.hpp>

#include <volk.h>

#include <memory>
#include <mutex>

namespace
{
    class [[nodiscard]] externally_synchronized_execution_port_t final
        : public vkrndr::execution_port_t
    {
    public:
        using vkrndr::execution_port_t::execution_port_t;

    protected:
        VkResult submit_impl(std::span<VkSubmitInfo const> const& submits,
            VkFence fence) override
        {
            std::scoped_lock g{mutex_};
            return vkrndr::execution_port_t::submit_impl(submits, fence);
        }

        VkResult present_impl(VkPresentInfoKHR const& present_info) override
        {
            std::scoped_lock g{mutex_};
            return vkrndr::execution_port_t::present_impl(present_info);
        }

    private:
        std::mutex mutex_;
    };
} // namespace

vkrndr::execution_port_t::execution_port_t(VkDevice const device,
    VkQueueFlags const queue_flags,
    uint32_t const queue_family,
    uint32_t const queue_index,
    bool const present_flag)
    : queue_flags_{queue_flags}
    , queue_family_{queue_family}
    , present_flag_{present_flag}
{
    vkGetDeviceQueue(device, queue_family, queue_index, &queue_);
}

bool vkrndr::execution_port_t::has_graphics() const noexcept
{
    return supports_flags(queue_flags_, VK_QUEUE_GRAPHICS_BIT);
}

bool vkrndr::execution_port_t::has_compute() const noexcept
{
    return supports_flags(queue_flags_, VK_QUEUE_COMPUTE_BIT);
}

bool vkrndr::execution_port_t::has_transfer() const noexcept
{
    return supports_flags(queue_flags_, VK_QUEUE_TRANSFER_BIT);
}

bool vkrndr::execution_port_t::has_present() const noexcept
{
    return present_flag_;
}

uint32_t vkrndr::execution_port_t::queue_family() const noexcept
{
    return queue_family_;
}

VkResult vkrndr::execution_port_t::submit(
    std::span<VkSubmitInfo const> const& submits,
    VkFence const fence)
{
    return submit_impl(submits, fence);
}

VkResult vkrndr::execution_port_t::present(VkPresentInfoKHR const& present_info)
{
    return present_impl(present_info);
}

VkResult vkrndr::execution_port_t::submit_impl(
    std::span<VkSubmitInfo const> const& submits,
    VkFence const fence)
{
    return vkQueueSubmit(queue_, count_cast(submits), submits.data(), fence);
}

VkResult vkrndr::execution_port_t::present_impl(
    VkPresentInfoKHR const& present_info)
{
    return vkQueuePresentKHR(queue_, &present_info);
}

std::unique_ptr<vkrndr::execution_port_t> vkrndr::create_execution_port(
    VkDevice device,
    VkQueueFlags queue_flags,
    uint32_t queue_family,
    uint32_t queue_index,
    bool present_flag,
    bool externally_synchronized)
{
    if (!externally_synchronized)
    {
        return std::make_unique<execution_port_t>(device,
            queue_flags,
            queue_family,
            queue_index,
            present_flag);
    }

    return std::make_unique<externally_synchronized_execution_port_t>(device,
        queue_flags,
        queue_family,
        queue_index,
        present_flag);
}
