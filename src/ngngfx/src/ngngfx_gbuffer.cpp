#include <ngngfx_gbuffer.hpp>

#include <vkrndr_device.hpp>
#include <vkrndr_utility.hpp>

ngngfx::gbuffer_t ngngfx::create_gbuffer(vkrndr::device_t& device,
    VkExtent2D const extent,
    VkSampleCountFlagBits samples,
    std::span<VkFormat const> const& formats,
    VkImageTiling const tiling,
    VkImageUsageFlags const usage,
    VkMemoryPropertyFlags const properties,
    VkImageAspectFlags const aspect_flags)
{
    std::vector<vkrndr::image_t> images;
    images.reserve(formats.size());

    for (auto const format : formats)
    {
        images.push_back(vkrndr::create_image_and_view(device,
            extent,
            1,
            samples,
            format,
            tiling,
            usage,
            properties,
            aspect_flags));
    }

    return {std::move(images)};
}

void ngngfx::destroy(vkrndr::device_t const* device, gbuffer_t* gbuffer)
{
    if (gbuffer)
    {
        for (auto& image : gbuffer->images)
        {
            destroy(device, &image);
        }
    }
}
