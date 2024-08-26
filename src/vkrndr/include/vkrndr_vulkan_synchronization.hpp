#ifndef VKRNDR_VULKAN_SYNCHRONIZATION_INCLUDED
#define VKRNDR_VULKAN_SYNCHRONIZATION_INCLUDED

#include <vulkan/vulkan_core.h>

namespace vkrndr
{
    struct vulkan_device;
} // namespace vkrndr

namespace vkrndr
{
    [[nodiscard]] VkSemaphore create_semaphore(vulkan_device const* device);

    [[nodiscard]] VkFence create_fence(vulkan_device const* device,
        bool set_signaled);
} // namespace vkrndr

#endif // !VKRNDR_VULKAN_SYNCHRONIZATION_INCLUDED
