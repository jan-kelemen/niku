#ifndef VKRNDR_EXECUTION_PORT_INCLUDED
#define VKRNDR_EXECUTION_PORT_INCLUDED

#include <volk.h>

#include <cstdint>
#include <span>

namespace vkrndr
{
    template<typename... Args>
    [[nodiscard]] constexpr bool supports_flags(VkQueueFlags const flags,
        Args... bits)
    {
        auto const all_bits{static_cast<VkQueueFlags>((bits | ...))};
        return (flags & all_bits) == all_bits;
    }

    class [[nodiscard]] execution_port_t final
    {
    public:
        execution_port_t() = default;

        execution_port_t(VkDevice device,
            VkQueueFlags queue_flags,
            uint32_t queue_family,
            uint32_t queue_index,
            bool present_flag);

        execution_port_t(execution_port_t const&) = default;

        execution_port_t(execution_port_t&&) noexcept = default;

    public:
        ~execution_port_t() = default;

    public:
        [[nodiscard]] bool has_graphics() const noexcept;

        [[nodiscard]] bool has_compute() const noexcept;

        [[nodiscard]] bool has_transfer() const noexcept;

        [[nodiscard]] bool has_present() const noexcept;

        [[nodiscard]] uint32_t queue_family() const noexcept;

        void submit(std::span<VkSubmitInfo const> const& submits,
            VkFence fence = VK_NULL_HANDLE);

        [[nodiscard]] VkResult present(VkPresentInfoKHR const& present_info);

        void wait_idle();

    public:
        [[nodiscard]] constexpr operator VkQueue() const noexcept;

        execution_port_t& operator=(execution_port_t const&) = default;

        execution_port_t& operator=(execution_port_t&&) = default;

    private:
        VkQueue queue_{VK_NULL_HANDLE};
        VkQueueFlags queue_flags_{};
        uint32_t queue_family_{};
        bool present_flag_{};
    };
} // namespace vkrndr

constexpr vkrndr::execution_port_t::operator VkQueue() const noexcept
{
    return queue_;
}

#endif
