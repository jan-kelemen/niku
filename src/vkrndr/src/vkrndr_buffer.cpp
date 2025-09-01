#include <vkrndr_buffer.hpp>

#include <vkrndr_debug_utils.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_utility.hpp>

#include <vma_impl.hpp>

#include <vulkan/utility/vk_struct_helper.hpp>

#include <bit>

void vkrndr::destroy(device_t const& device, buffer_t const& buffer)
{
    vmaDestroyBuffer(device.allocator, buffer, buffer.allocation);
}

VkDeviceAddress vkrndr::device_address(device_t const& device,
    buffer_t const& buffer)
{
    VkBufferDeviceAddressInfo const da{
        .sType = vku::GetSType<VkBufferDeviceAddressInfo>(),
        .buffer = buffer,
    };

    return vkGetBufferDeviceAddress(device, &da);
}

vkrndr::buffer_t vkrndr::create_buffer(device_t const& device,
    buffer_create_info_t const& create_info)
{
    buffer_t rv{.size = create_info.size};

    VkBufferCreateInfo buffer_info{
        .sType = vku::GetSType<VkBufferCreateInfo>(),
        .pNext = create_info.chain,
        .flags = create_info.buffer_flags,
        .size = create_info.size,
        .usage = create_info.usage,
    };
    if (!create_info.sharing_queue_families.empty())
    {
        buffer_info.sharingMode = VK_SHARING_MODE_CONCURRENT;
        buffer_info.queueFamilyIndexCount =
            count_cast(create_info.sharing_queue_families.size());
        buffer_info.pQueueFamilyIndices =
            create_info.sharing_queue_families.data();
    }

    VmaAllocationCreateInfo vma_info{.flags = create_info.allocation_flags,
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = create_info.required_memory_flags,
        .preferredFlags = create_info.preferred_memory_flags,
        .memoryTypeBits = create_info.memory_type_bits,
        .pool = create_info.pool,
        .priority = create_info.priority};
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

    check_result(vmaCreateBuffer(device.allocator,
        &buffer_info,
        &vma_info,
        &rv.handle,
        &rv.allocation,
        nullptr));

    if (create_info.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
    {
        rv.device_address = device_address(device, rv);
    }

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
    std::string_view const name)
{
    object_name(device,
        VK_OBJECT_TYPE_BUFFER,
        std::bit_cast<uint64_t>(buffer.handle),
        name);
}
