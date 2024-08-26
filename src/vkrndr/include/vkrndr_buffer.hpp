#ifndef VKRNDR_BUFFER_INCLUDED
#define VKRNDR_BUFFER_INCLUDED

#include <vma_impl.hpp>

#include <vulkan/vulkan_core.h>

namespace vkrndr
{
    struct device_t;
} // namespace vkrndr

namespace vkrndr
{
    struct [[nodiscard]] buffer_t final
    {
        VkBuffer buffer{VK_NULL_HANDLE};
        VkDeviceSize size{};
        VmaAllocation allocation{VK_NULL_HANDLE};
    };

    void destroy(device_t const* device, buffer_t* buffer);

    buffer_t create_buffer(device_t const& device,
        VkDeviceSize size,
        VkBufferCreateFlags usage,
        VkMemoryPropertyFlags memory_properties);
} // namespace vkrndr
#endif
