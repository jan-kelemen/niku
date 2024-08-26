#ifndef VKRNDR_QUEUE_INCLUDED
#define VKRNDR_QUEUE_INCLUDED

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <optional>

namespace vkrndr
{
    struct device_t;
} // namespace vkrndr

namespace vkrndr
{
    struct [[nodiscard]] queue_t final
    {
        uint32_t family{};
        VkQueue queue{VK_NULL_HANDLE};
    };

    queue_t
    create_queue(device_t const* device, uint32_t family, uint32_t queue_index);

    struct [[nodiscard]] queue_families final
    {
        std::optional<uint32_t> present_family;
        std::optional<uint32_t> dedicated_transfer_family;
    };

    queue_families find_queue_families(VkPhysicalDevice physical_device,
        VkSurfaceKHR surface);

    [[nodiscard]] VkCommandPool create_command_pool(device_t const& device,
        uint32_t queue_family);

} // namespace vkrndr

#endif
