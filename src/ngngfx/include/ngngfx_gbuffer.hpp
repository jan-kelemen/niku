#ifndef NGNGFX_GBUFFER_INCLUDED
#define NGNGFX_GBUFFER_INCLUDED

#include <vkrndr_image.hpp>

#include <vma_impl.hpp>

#include <volk.h>

#include <cstdint>
#include <span>
#include <vector>

namespace vkrndr
{
    struct device_t;
} // namespace vkrndr

namespace ngngfx
{
    struct [[nodiscard]] gbuffer_t final
    {
        std::vector<vkrndr::image_t> images;
    };

    struct [[nodiscard]] gbuffer_create_info_t final
    {
        void const* chain{};
        VkImageCreateFlags image_flags{};
        std::span<VkFormat const> formats;
        VkExtent2D extent{};
        uint32_t mip_levels{1};
        uint32_t array_layers{1};
        VkSampleCountFlagBits samples{VK_SAMPLE_COUNT_1_BIT};
        VkImageTiling tiling{};
        VkImageUsageFlags usage{};
        std::span<uint32_t const> sharing_queue_families;
        VkImageLayout initial_layout{VK_IMAGE_LAYOUT_UNDEFINED};
        VmaAllocationCreateFlags allocation_flags{};
        VkMemoryPropertyFlags required_memory_flags{};
        VkMemoryPropertyFlags preferred_memory_flags{};
        uint32_t memory_type_bits{};
        VmaPool pool{VK_NULL_HANDLE};
        float priority{};
    };

    gbuffer_t create_gbuffer(vkrndr::device_t const& device,
        gbuffer_create_info_t const& create_info,
        VkImageAspectFlags aspect_flags);

    void destroy(vkrndr::device_t const* device, gbuffer_t* gbuffer);
} // namespace ngngfx
#endif
