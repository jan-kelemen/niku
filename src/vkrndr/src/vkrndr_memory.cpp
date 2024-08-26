#include <vkrndr_memory.hpp>

#include <vkrndr_buffer.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_utility.hpp>

#include <vma_impl.hpp>

vkrndr::mapped_memory_t vkrndr::map_memory(device_t const& device,
    buffer_t const& buffer)
{
    void* mapped; // NOLINT
    check_result(vmaMapMemory(device.allocator, buffer.allocation, &mapped));
    return {buffer.allocation, mapped};
}

void vkrndr::unmap_memory(device_t const& device, mapped_memory_t* memory)
{
    vmaUnmapMemory(device.allocator, memory->allocation);
}
