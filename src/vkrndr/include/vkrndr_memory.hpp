#ifndef VKRNDR_MEMORY_INCLUDED
#define VKRNDR_MEMORY_INCLUDED

#include <vma_impl.hpp>

#include <volk.h>

#include <cstddef>

namespace vkrndr
{
    struct device_t;
    struct buffer_t;
} // namespace vkrndr

namespace vkrndr
{
    struct [[nodiscard]] memory_region_t final
    {
        VkDeviceSize offset{};
        VkDeviceSize size{};
    };

    struct [[nodiscard]] mapped_memory_t final
    {
        VmaAllocation allocation{VK_NULL_HANDLE};
        void* mapped_memory{nullptr};

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

    mapped_memory_t map_memory(device_t const& device, buffer_t const& buffer);

    void unmap_memory(device_t const& device, mapped_memory_t* memory);
} // namespace vkrndr

#endif
