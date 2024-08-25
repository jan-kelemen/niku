#ifndef VKRNDR_VULKAN_MEMORY_INCLUDED
#define VKRNDR_VULKAN_MEMORY_INCLUDED

#include <vma_impl.hpp>

#include <vulkan/vulkan_core.h>

#include <cstddef>

namespace vkrndr
{
    struct vulkan_device;
    struct vulkan_buffer;
} // namespace vkrndr

namespace vkrndr
{
    struct [[nodiscard]] memory_region final
    {
        VkDeviceSize offset{};
        VkDeviceSize size{};
    };

    struct [[nodiscard]] mapped_memory final
    {
        VmaAllocation allocation;
        void* mapped_memory;

        template<typename T>
        [[nodiscard]] T* as(size_t offset = 0)
        {
            // NOLINTBEGIN
            auto* const byte_view{reinterpret_cast<std::byte*>(mapped_memory)};
            return reinterpret_cast<T*>(byte_view + offset);
            // NOLINTEND
        }

        template<typename T>
        [[nodiscard]] T const* as(size_t offset = 0) const
        {
            // NOLINTBEGIN
            auto const* const byte_view{
                reinterpret_cast<std::byte const*>(mapped_memory)};
            return reinterpret_cast<T const*>(byte_view + offset);
            // NOLINTEND
        }
    };

    mapped_memory map_memory(vulkan_device const& device,
        vulkan_buffer const& buffer);

    void unmap_memory(vulkan_device const& device, mapped_memory* memory);
} // namespace vkrndr

#endif // !VKRNDR_VULKAN_MEMORY_INCLUDED
