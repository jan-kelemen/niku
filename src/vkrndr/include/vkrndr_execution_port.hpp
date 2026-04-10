#ifndef VKRNDR_EXECUTION_PORT_INCLUDED
#define VKRNDR_EXECUTION_PORT_INCLUDED

#include <volk.h>

#include <cstdint>
#include <memory>
#include <span>

namespace vkrndr
{
    class [[nodiscard]] execution_port_t
    {
    public:
        execution_port_t() = default;

        execution_port_t(VkDevice device,
            VkQueueFlags queue_flags,
            uint32_t queue_family,
            uint32_t queue_index,
            bool present_flag);

        execution_port_t(execution_port_t const&) = delete;

        execution_port_t(execution_port_t&&) noexcept = delete;

    public:
        virtual ~execution_port_t() = default;

    public:
        [[nodiscard]] bool has_graphics() const noexcept;

        [[nodiscard]] bool has_compute() const noexcept;

        [[nodiscard]] bool has_transfer() const noexcept;

        [[nodiscard]] bool has_present() const noexcept;

        [[nodiscard]] uint32_t queue_family() const noexcept;

        [[nodiscard]] VkResult submit(
            std::span<VkSubmitInfo const> const& submits,
            VkFence fence = VK_NULL_HANDLE);

        [[nodiscard]] VkResult present(VkPresentInfoKHR const& present_info);

    public:
        [[nodiscard]] constexpr operator VkQueue() const noexcept;

        execution_port_t& operator=(execution_port_t const&) = delete;

        execution_port_t& operator=(execution_port_t&&) = delete;

    protected:
        virtual VkResult submit_impl(
            std::span<VkSubmitInfo const> const& submits,
            VkFence fence);

        virtual VkResult present_impl(VkPresentInfoKHR const& present_info);

    private:
        VkQueue queue_{VK_NULL_HANDLE};
        VkQueueFlags queue_flags_{};
        uint32_t queue_family_{};
        bool present_flag_{};
    };

    [[nodiscard]] std::unique_ptr<execution_port_t> create_execution_port(
        VkDevice device,
        VkQueueFlags queue_flags,
        uint32_t queue_family,
        uint32_t queue_index,
        bool present_flag,
        bool externally_synchronized);
} // namespace vkrndr

constexpr vkrndr::execution_port_t::operator VkQueue() const noexcept
{
    return queue_;
}

#endif
