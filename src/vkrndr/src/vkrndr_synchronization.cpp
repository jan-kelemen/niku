#include <vkrndr_synchronization.hpp>

#include <vkrndr_device.hpp>
#include <vkrndr_utility.hpp>

#include <cppext_numeric.hpp>

VkSemaphore vkrndr::create_semaphore(device_t const& device)
{
    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkSemaphore rv; // NOLINT
    check_result(vkCreateSemaphore(device, &semaphore_info, nullptr, &rv));

    return rv;
}

VkFence vkrndr::create_fence(device_t const& device, bool const set_signaled)
{
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (set_signaled)
    {
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    }

    VkFence rv; // NOLINT
    check_result(vkCreateFence(device, &fence_info, nullptr, &rv));

    return rv;
}

void vkrndr::wait_for(VkCommandBuffer command_buffer,
    std::span<VkMemoryBarrier2 const> const& global_memory_barriers,
    std::span<VkBufferMemoryBarrier2 const> const& buffer_barriers,
    std::span<VkImageMemoryBarrier2 const> const& image_barriers)
{
    VkDependencyInfo dependency{};
    dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;

    if (!global_memory_barriers.empty())
    {
        dependency.memoryBarrierCount =
            cppext::narrow<uint32_t>(global_memory_barriers.size());
        dependency.pMemoryBarriers = global_memory_barriers.data();
    }

    if (!buffer_barriers.empty())
    {
        dependency.bufferMemoryBarrierCount =
            cppext::narrow<uint32_t>(buffer_barriers.size());
        dependency.pBufferMemoryBarriers = buffer_barriers.data();
    }

    if (!image_barriers.empty())
    {
        dependency.imageMemoryBarrierCount =
            cppext::narrow<uint32_t>(image_barriers.size());
        dependency.pImageMemoryBarriers = image_barriers.data();
    }

    vkCmdPipelineBarrier2(command_buffer, &dependency);
}
