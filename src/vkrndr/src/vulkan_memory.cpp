#include <vulkan_memory.hpp>

#include <vulkan_buffer.hpp>
#include <vulkan_device.hpp>
#include <vulkan_utility.hpp>

#include <vma_impl.hpp>

vkrndr::mapped_memory vkrndr::map_memory(vulkan_device const& device,
    vulkan_buffer const& buffer)
{
    void* mapped; // NOLINT
    check_result(vmaMapMemory(device.allocator, buffer.allocation, &mapped));
    return {buffer.allocation, mapped};
}

void vkrndr::unmap_memory(vulkan_device const& device, mapped_memory* memory)
{
    vmaUnmapMemory(device.allocator, memory->allocation);
}
