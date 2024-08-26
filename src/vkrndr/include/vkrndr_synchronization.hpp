#ifndef VKRNDR_SYNCHRONIZATION_INCLUDED
#define VKRNDR_SYNCHRONIZATION_INCLUDED

#include <vulkan/vulkan_core.h>

namespace vkrndr
{
    struct device_t;
} // namespace vkrndr

namespace vkrndr
{
    [[nodiscard]] VkSemaphore create_semaphore(device_t const* device);

    [[nodiscard]] VkFence create_fence(device_t const* device,
        bool set_signaled);
} // namespace vkrndr

#endif // !VKRNDR_SYNCHRONIZATION_INCLUDED
