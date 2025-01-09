#ifndef NGNGFX_GBUFFER_INCLUDED
#define NGNGFX_GBUFFER_INCLUDED

#include <vkrndr_image.hpp>

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

    gbuffer_t create_gbuffer(vkrndr::device_t& device,
        VkExtent2D extent,
        VkSampleCountFlagBits samples,
        std::span<VkFormat const> const& formats,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkImageAspectFlags aspect_flags);

    void destroy(vkrndr::device_t const* device, gbuffer_t* gbuffer);
} // namespace ngngfx
#endif
