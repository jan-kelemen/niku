#ifndef VKRNDR_BUFFER_INCLUDED
#define VKRNDR_BUFFER_INCLUDED

#include <vma_impl.hpp>

#include <volk.h>

#include <cstdint>
#include <span>
#include <string_view>

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

    struct [[nodiscard]] buffer_create_info_t final
    {
        void const* chain{};
        VkBufferCreateFlags buffer_flags{};
        VkDeviceSize size{};
        VkBufferUsageFlags usage{};
        std::span<uint32_t const> sharing_queue_families;
        VmaAllocationCreateFlags allocation_flags{};
        VkMemoryPropertyFlags required_memory_flags{};
        VkMemoryPropertyFlags preferred_memory_flags{};
        uint32_t memory_type_bits{};
        VmaPool pool{VK_NULL_HANDLE};
        float priority{};
    };

    buffer_t create_buffer(device_t const& device,
        buffer_create_info_t const& create_info);

    buffer_t create_staging_buffer(device_t const& device, VkDeviceSize size);

    void object_name(device_t const& device,
        buffer_t const& buffer,
        std::string_view name);
} // namespace vkrndr
#endif
