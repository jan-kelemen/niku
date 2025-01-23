#include <vkrndr_buffer.hpp>

#include <vkrndr_debug_utils.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_utility.hpp>

#include <vma_impl.hpp>

#include <bit>

void vkrndr::destroy(device_t const* device, buffer_t* const buffer)
{
    if (buffer)
    {
        vmaDestroyBuffer(device->allocator, buffer->buffer, buffer->allocation);
    }
}

vkrndr::buffer_t vkrndr::create_buffer(device_t const& device,
    buffer_create_info_t const& create_info)
{
    buffer_t rv{};
    rv.size = create_info.size;

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.pNext = create_info.chain;
    buffer_info.flags = create_info.buffer_flags;
    buffer_info.size = create_info.size;
    buffer_info.usage = create_info.usage;
    if (!create_info.sharing_queue_families.empty())
    {
        buffer_info.sharingMode = VK_SHARING_MODE_CONCURRENT;
        buffer_info.queueFamilyIndexCount =
            count_cast(create_info.sharing_queue_families.size());
        buffer_info.pQueueFamilyIndices =
            create_info.sharing_queue_families.data();
    }

    VmaAllocationCreateInfo vma_info{};
    vma_info.flags = create_info.allocation_flags;
    vma_info.usage = VMA_MEMORY_USAGE_AUTO;
    if (create_info.required_memory_flags &
        VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT)
    {
        vma_info.usage = VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED;
    }
    else if (create_info.preferred_memory_flags &
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    {
        vma_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    }
    vma_info.requiredFlags = create_info.required_memory_flags;
    vma_info.preferredFlags = create_info.preferred_memory_flags;
    vma_info.memoryTypeBits = create_info.memory_type_bits;
    vma_info.pool = create_info.pool;
    vma_info.priority = create_info.priority;

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
        {.size = size,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .allocation_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
            .required_memory_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT});
}

void vkrndr::object_name(device_t const& device,
    buffer_t const& buffer,
    std::string_view name)
{
    object_name(device.logical,
        VK_OBJECT_TYPE_BUFFER,
        std::bit_cast<uint64_t>(buffer.buffer),
        name);
}
