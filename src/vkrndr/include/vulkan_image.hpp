#ifndef VKRNDR_VULKAN_IMAGE_INCLUDED
#define VKRNDR_VULKAN_IMAGE_INCLUDED

#include <vma_impl.hpp>

#include <vulkan/vulkan_core.h>

#include <cstdint>

namespace vkrndr
{
    struct vulkan_device;
} // namespace vkrndr

namespace vkrndr
{
    struct [[nodiscard]] vulkan_image final
    {
        VkImage image{VK_NULL_HANDLE};
        VmaAllocation allocation{VK_NULL_HANDLE};
        VkImageView view{VK_NULL_HANDLE};
        VkFormat format{};
        VkSampleCountFlags sample_count{VK_SAMPLE_COUNT_1_BIT};
        uint32_t mip_levels{1};
        VkExtent2D extent{};
    };

    void destroy(vulkan_device const* device, vulkan_image* image);

    vulkan_image create_image(vulkan_device const& device,
        VkExtent2D extent,
        uint32_t mip_levels,
        VkSampleCountFlagBits samples,
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties);

    [[nodiscard]] VkImageView create_image_view(vulkan_device const& device,
        VkImage image,
        VkFormat format,
        VkImageAspectFlags aspect_flags,
        uint32_t mip_levels);

    vulkan_image create_image_and_view(vulkan_device const& device,
        VkExtent2D extent,
        uint32_t mip_levels,
        VkSampleCountFlagBits samples,
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkImageAspectFlags aspect_flags);
} // namespace vkrndr

#endif
