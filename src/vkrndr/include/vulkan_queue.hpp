#ifndef VKRNDR_VULKAN_QUEUE_INCLUDED
#define VKRNDR_VULKAN_QUEUE_INCLUDED

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <optional>

namespace vkrndr
{
    struct vulkan_device;
} // namespace vkrndr

namespace vkrndr
{
    struct [[nodiscard]] vulkan_queue final
    {
        uint32_t family{};
        VkQueue queue{VK_NULL_HANDLE};
    };

    vulkan_queue create_queue(vulkan_device const* device,
        uint32_t family,
        uint32_t queue_index);

    struct [[nodiscard]] queue_families final
    {
        std::optional<uint32_t> present_family;
        std::optional<uint32_t> dedicated_transfer_family;
    };

    queue_families find_queue_families(VkPhysicalDevice physical_device,
        VkSurfaceKHR surface);

    [[nodiscard]] VkCommandPool create_command_pool(vulkan_device const& device,
        uint32_t queue_family);

} // namespace vkrndr

#endif // !VKRNDR_VULKAN_QUEUE_INCLUDED
