#include <vulkan_image.hpp>

#include <vulkan_device.hpp>
#include <vulkan_utility.hpp>

#include <vma_impl.hpp>

void vkrndr::destroy(vulkan_device const* device, vulkan_image* const image)
{
    if (image)
    {
        vkDestroyImageView(device->logical, image->view, nullptr);
        vmaDestroyImage(device->allocator, image->image, image->allocation);
    }
}

vkrndr::vulkan_image vkrndr::create_image(vulkan_device const& device,
    VkExtent2D const extent,
    uint32_t const mip_levels,
    VkSampleCountFlagBits const samples,
    VkFormat const format,
    VkImageTiling const tiling,
    VkImageUsageFlags const usage,
    VkMemoryPropertyFlags const properties)
{
    vulkan_image rv;
    rv.format = format;
    rv.sample_count = samples;
    rv.mip_levels = mip_levels;
    rv.extent = extent;

    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent = {extent.width, extent.height, 1};
    image_info.mipLevels = mip_levels;
    image_info.arrayLayers = 1;
    image_info.format = format;
    image_info.tiling = tiling;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = usage;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.samples = samples;
    image_info.flags = 0;

    VmaAllocationCreateInfo vma_info{};
    vma_info.usage = (usage & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        ? VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        : VMA_MEMORY_USAGE_AUTO;
    if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT ||
        properties & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
    {
        vma_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    }

    check_result(vmaCreateImage(device.allocator,
        &image_info,
        &vma_info,
        &rv.image,
        &rv.allocation,
        nullptr));

    return rv;
}

[[nodiscard]]
VkImageView vkrndr::create_image_view(vulkan_device const& device,
    VkImage const image,
    VkFormat const format,
    VkImageAspectFlags const aspect_flags,
    uint32_t const mip_levels)
{
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = format;
    view_info.subresourceRange.aspectMask = aspect_flags;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = mip_levels;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    VkImageView imageView; // NOLINT
    check_result(
        vkCreateImageView(device.logical, &view_info, nullptr, &imageView));
    return imageView;
}

vkrndr::vulkan_image vkrndr::create_image_and_view(vulkan_device const& device,
    VkExtent2D const extent,
    uint32_t const mip_levels,
    VkSampleCountFlagBits const samples,
    VkFormat const format,
    VkImageTiling const tiling,
    VkImageUsageFlags const usage,
    VkMemoryPropertyFlags const properties,
    VkImageAspectFlags const aspect_flags)
{
    vulkan_image rv{create_image(device,
        extent,
        mip_levels,
        samples,
        format,
        tiling,
        usage,
        properties)};
    rv.view =
        create_image_view(device, rv.image, format, aspect_flags, mip_levels);
    return rv;
}
