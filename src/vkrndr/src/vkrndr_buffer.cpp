#include <vkrndr_buffer.hpp>

#include <vkrndr_device.hpp>
#include <vkrndr_utility.hpp>

#include <vma_impl.hpp>

void vkrndr::destroy(device_t const* device, buffer_t* const buffer)
{
    if (buffer)
    {
        vmaDestroyBuffer(device->allocator, buffer->buffer, buffer->allocation);
    }
}

vkrndr::buffer_t vkrndr::create_buffer(device_t const& device,
    VkDeviceSize const size,
    VkBufferCreateFlags const usage,
    VkMemoryPropertyFlags const memory_properties)
{
    buffer_t rv{};
    rv.size = size;

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo vma_info{};
    vma_info.usage = (usage & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        ? VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        : VMA_MEMORY_USAGE_AUTO;
    if (memory_properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT ||
        memory_properties & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
    {
        vma_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    }

    check_result(vmaCreateBuffer(device.allocator,
        &buffer_info,
        &vma_info,
        &rv.buffer,
        &rv.allocation,
        nullptr));

    return rv;
}

vkrndr::buffer_t vkrndr::create_staging_buffer(device_t const& device,
    VkDeviceSize const size)
{
    return create_buffer(device,
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}
