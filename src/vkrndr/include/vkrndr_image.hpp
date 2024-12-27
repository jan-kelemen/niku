#ifndef VKRNDR_IMAGE_INCLUDED
#define VKRNDR_IMAGE_INCLUDED

#include <vkrndr_synchronization.hpp>

#include <vma_impl.hpp>

#include <volk.h>

#include <cstdint>

namespace vkrndr
{
    struct device_t;
} // namespace vkrndr

namespace vkrndr
{
    struct [[nodiscard]] image_t final
    {
        VkImage image{VK_NULL_HANDLE};
        VmaAllocation allocation{VK_NULL_HANDLE};
        VkImageView view{VK_NULL_HANDLE};
        VkFormat format{};
        VkSampleCountFlags sample_count{VK_SAMPLE_COUNT_1_BIT};
        uint32_t mip_levels{1};
        VkExtent2D extent{};
    };

    void destroy(device_t const* device, image_t* image);

    image_t create_image(device_t const& device,
        VkExtent2D extent,
        uint32_t mip_levels,
        VkSampleCountFlagBits samples,
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties);

    [[nodiscard]] VkImageView create_image_view(device_t const& device,
        VkImage image,
        VkFormat format,
        VkImageAspectFlags aspect_flags,
        uint32_t mip_levels,
        uint32_t base_mip_level = 0);

    image_t create_image_and_view(device_t const& device,
        VkExtent2D extent,
        uint32_t mip_levels,
        VkSampleCountFlagBits samples,
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkImageAspectFlags aspect_flags);

    [[nodiscard]] constexpr VkImageMemoryBarrier2 image_barrier(
        vkrndr::image_t const& image,
        VkImageAspectFlags const aspect = VK_IMAGE_ASPECT_COLOR_BIT)
    {
        return image_barrier(image.image,
            whole_resource(aspect, image.mip_levels, 1));
    }
} // namespace vkrndr

#endif
