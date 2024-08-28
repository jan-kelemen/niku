#ifndef VKRNDR_EXECUTION_PORT_INCLUDED
#define VKRNDR_EXECUTION_PORT_INCLUDED

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <span>

namespace vkrndr
{
    template<typename... Args>
    [[nodiscard]] constexpr bool supports_flags(VkQueueFlags flags,
        Args... bits)
    {
        auto const all_bits{static_cast<VkQueueFlags>((bits | ...))};
        return static_cast<VkQueueFlags>(flags & all_bits) == all_bits;
    }

    class [[nodiscard]] execution_port_t final
    {
    public:
        execution_port_t() = default;

        execution_port_t(VkDevice device,
            VkQueueFlags queue_flags,
            uint32_t queue_family,
            uint32_t queue_index);

        execution_port_t(execution_port_t const&) = default;

        execution_port_t(execution_port_t&&) noexcept = default;

    public:
        ~execution_port_t() = default;

    public:
        [[nodiscard]] bool has_graphics() const;

        [[nodiscard]] bool has_compute() const;

        [[nodiscard]] bool has_transfer() const;

        [[nodiscard]] VkQueue queue() const;

        [[nodiscard]] uint32_t queue_family() const;

        void submit(std::span<VkSubmitInfo const> submits,
            VkFence fence = VK_NULL_HANDLE);

        [[nodiscard]] VkResult present(VkPresentInfoKHR const& present_info);

        void wait_idle();

    public:
        execution_port_t& operator=(execution_port_t const&) = default;

        execution_port_t& operator=(execution_port_t&&) = default;

    private:
        VkQueue queue_{VK_NULL_HANDLE};
        VkQueueFlags queue_flags_{};
        uint32_t queue_family_{};
    };
} // namespace vkrndr
#endif
