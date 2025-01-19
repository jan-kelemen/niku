#ifndef VKRNDR_SYNCHRONIZATION_INCLUDED
#define VKRNDR_SYNCHRONIZATION_INCLUDED

#include <volk.h>

#include <concepts>
#include <cstdint>
#include <span>

namespace vkrndr
{
    struct device_t;
} // namespace vkrndr

namespace vkrndr
{
    [[nodiscard]] VkSemaphore create_semaphore(device_t const& device);

    [[nodiscard]] VkFence create_fence(device_t const& device,
        bool set_signaled);

    [[nodiscard]] constexpr VkMemoryBarrier2 memory_barrier()
    {
        return {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2, .pNext = nullptr};
    }

    [[nodiscard]] constexpr VkBufferMemoryBarrier2 buffer_barrier(
        VkBuffer const buffer,
        VkDeviceSize const offset = 0,
        VkDeviceSize const size = VK_WHOLE_SIZE)
    {
        return {.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = buffer,
            .offset = offset,
            .size = size};
    }

    [[nodiscard]] constexpr VkImageSubresourceRange whole_resource(
        VkImageAspectFlags const aspect = VK_IMAGE_ASPECT_COLOR_BIT,
        uint32_t const levels = 1,
        uint32_t const layers = 1)
    {
        return {.aspectMask = aspect,
            .baseMipLevel = 0,
            .levelCount = levels,
            .baseArrayLayer = 0,
            .layerCount = layers};
    }

    [[nodiscard]] constexpr VkImageMemoryBarrier2 image_barrier(
        VkImage const image,
        VkImageSubresourceRange const& resource_range = whole_resource())
    {
        return {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image,
            .subresourceRange = resource_range};
    }

    template<typename T>
    [[nodiscard]] constexpr T on_stage(T const& barrier,
        VkPipelineStageFlags2 const from,
        VkPipelineStageFlags2 const to)
    requires(std::same_as<T, VkMemoryBarrier2> ||
        std::same_as<T, VkBufferMemoryBarrier2> ||
        std::same_as<T, VkImageMemoryBarrier2>)
    {
        T rv{barrier};
        rv.srcStageMask = from;
        rv.dstStageMask = to;

        return rv;
    }

    template<typename T>
    [[nodiscard]] constexpr T on_stage(T const& barrier,
        VkPipelineStageFlags2 const stage)
    requires(std::same_as<T, VkMemoryBarrier2> ||
        std::same_as<T, VkBufferMemoryBarrier2> ||
        std::same_as<T, VkImageMemoryBarrier2>)
    {
        return on_stage(barrier, stage, stage);
    }

    template<typename T>
    [[nodiscard]] constexpr T with_access(T const& barrier,
        VkAccessFlags2 const from,
        VkAccessFlags2 const to)
    requires(std::same_as<T, VkMemoryBarrier2> ||
        std::same_as<T, VkBufferMemoryBarrier2> ||
        std::same_as<T, VkImageMemoryBarrier2>)
    {
        T rv{barrier};
        rv.srcAccessMask = from;
        rv.dstAccessMask = to;

        return rv;
    }

    template<typename T>
    [[nodiscard]] constexpr T with_access(T const& barrier,
        VkAccessFlags2 const access)
    requires(std::same_as<T, VkMemoryBarrier2> ||
        std::same_as<T, VkBufferMemoryBarrier2> ||
        std::same_as<T, VkImageMemoryBarrier2>)
    {
        return with_access(barrier, access, access);
    }

    template<typename T>
    [[nodiscard]] constexpr T with_ownership_transfer(T const& barrier,
        uint32_t const from,
        uint32_t const to)
    requires(std::same_as<T, VkBufferMemoryBarrier2> ||
        std::same_as<T, VkImageMemoryBarrier2>)
    {
        auto rv{barrier};
        rv.srcQueueFamilyIndex = from;
        rv.dstQueueFamilyIndex = to;

        return rv;
    }

    [[nodiscard]] constexpr VkImageMemoryBarrier2 with_layout(
        VkImageMemoryBarrier2 const& barrier,
        VkImageLayout const from,
        VkImageLayout const to)
    {
        auto rv{barrier};
        rv.oldLayout = from;
        rv.newLayout = to;

        return rv;
    }

    [[nodiscard]] constexpr VkImageMemoryBarrier2
    to_layout(VkImageMemoryBarrier2 const& barrier, VkImageLayout const to)
    {
        return with_layout(barrier, VK_IMAGE_LAYOUT_UNDEFINED, to);
    }

    void wait_for(VkCommandBuffer command_buffer,
        std::span<VkMemoryBarrier2 const> const& global_memory_barriers,
        std::span<VkBufferMemoryBarrier2 const> const& buffer_barriers,
        std::span<VkImageMemoryBarrier2 const> const& image_barriers);
} // namespace vkrndr

#endif // !VKRNDR_SYNCHRONIZATION_INCLUDED
