#include <vkrndr_vulkan_memory.hpp>

#include <vkrndr_vulkan_buffer.hpp>
#include <vkrndr_vulkan_device.hpp>
#include <vkrndr_vulkan_utility.hpp>

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
