#include <vkrndr_cubemap.hpp>

#include <vkrndr_device.hpp>
#include <vkrndr_utility.hpp>

#include <vma_impl.hpp>

#include <volk.h>

void vkrndr::destroy(device_t const* const device, cubemap_t* const cubemap)
{
    if (cubemap)
    {
        vkDestroyImageView(device->logical, cubemap->view, nullptr);

        for (VkImageView const face_view : cubemap->face_views)
        {
            vkDestroyImageView(device->logical, face_view, nullptr);
        }

        vmaDestroyImage(device->allocator, cubemap->image, cubemap->allocation);
    }
}

vkrndr::cubemap_t vkrndr::create_cubemap(device_t const& device,
    uint32_t const dimension,
    uint32_t const mip_levels,
    VkSampleCountFlagBits const samples,
    VkFormat const format,
    VkImageTiling const tiling,
    VkImageUsageFlags const usage,
    VkMemoryPropertyFlags const properties)
{
    cubemap_t rv;
    rv.format = format;
    rv.sample_count = samples;
    rv.mip_levels = mip_levels;
    rv.extent = vkrndr::to_extent(dimension, dimension);

    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent = {dimension, dimension, 1};
    image_info.mipLevels = mip_levels;
    image_info.arrayLayers = 6;
    image_info.format = format;
    image_info.tiling = tiling;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = usage;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.samples = samples;
    image_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

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

    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = rv.image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    view_info.format = format;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = mip_levels;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 6;

    check_result(
        vkCreateImageView(device.logical, &view_info, nullptr, &rv.view));

    for (uint32_t i{}; i != 6; ++i)
    {
        VkImageViewCreateInfo face_view_info{};
        face_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        face_view_info.image = rv.image;
        face_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        face_view_info.format = format;
        face_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        face_view_info.subresourceRange.baseMipLevel = 0;
        face_view_info.subresourceRange.levelCount = mip_levels;
        face_view_info.subresourceRange.baseArrayLayer = i;
        face_view_info.subresourceRange.layerCount = 1;

        check_result(vkCreateImageView(device.logical,
            &face_view_info,
            nullptr,
            &rv.face_views[i]));
    }

    return rv;
}

std::array<VkImageView, 6> vkrndr::face_views_for_mip(device_t const& device,
    vkrndr::cubemap_t const& cubemap,
    uint32_t const mip_level)
{
    std::array<VkImageView, 6> rv; // NOLINT

    for (uint32_t i{}; i != rv.size(); ++i)
    {
        VkImageViewCreateInfo face_view_info{};
        face_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        face_view_info.image = cubemap.image;
        face_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        face_view_info.format = cubemap.format;
        face_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        face_view_info.subresourceRange.baseMipLevel = mip_level;
        face_view_info.subresourceRange.levelCount = 1;
        face_view_info.subresourceRange.baseArrayLayer = i;
        face_view_info.subresourceRange.layerCount = 1;

        check_result(vkCreateImageView(device.logical,
            &face_view_info,
            nullptr,
            &rv[i]));
    }

    return rv;
}
