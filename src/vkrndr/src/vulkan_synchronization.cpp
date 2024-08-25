#include <vulkan_synchronization.hpp>

#include <vulkan_device.hpp>
#include <vulkan_utility.hpp>

VkSemaphore vkrndr::create_semaphore(vulkan_device const* const device)
{
    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkSemaphore rv; // NOLINT
    check_result(
        vkCreateSemaphore(device->logical, &semaphore_info, nullptr, &rv));

    return rv;
}

VkFence vkrndr::create_fence(vulkan_device const* const device,
    bool const set_signaled)
{
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (set_signaled)
    {
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    }

    VkFence rv; // NOLINT
    check_result(vkCreateFence(device->logical, &fence_info, nullptr, &rv));

    return rv;
}
