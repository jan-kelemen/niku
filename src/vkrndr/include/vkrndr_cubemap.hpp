#ifndef VKRNDR_CUBEMAP_INCLUDED
#define VKRNDR_CUBEMAP_INCLUDED

#include <vma_impl.hpp>

#include <volk.h>

#include <array>
#include <cstdint>

namespace vkrndr
{
    struct device_t;
} // namespace vkrndr

namespace vkrndr
{
    struct [[nodiscard]] cubemap_t final
    {
        VkImage image{VK_NULL_HANDLE};
        VmaAllocation allocation{VK_NULL_HANDLE};
        VkImageView view{VK_NULL_HANDLE};
        std::array<VkImageView, 6> face_views{VK_NULL_HANDLE,
            VK_NULL_HANDLE,
            VK_NULL_HANDLE,
            VK_NULL_HANDLE,
            VK_NULL_HANDLE,
            VK_NULL_HANDLE};
        VkFormat format{};
        VkSampleCountFlags sample_count{VK_SAMPLE_COUNT_1_BIT};
        uint32_t mip_levels{1};
        VkExtent2D extent{};
    };

    void destroy(device_t const* device, cubemap_t* cubemap);

    cubemap_t create_cubemap(device_t const& device,
        uint32_t dimension,
        uint32_t mip_levels,
        VkSampleCountFlagBits samples,
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties);
} // namespace vkrndr
#endif
