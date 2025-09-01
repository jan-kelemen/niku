#include <vkrndr_image.hpp>

#include <vkrndr_debug_utils.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_utility.hpp>

#include <vma_impl.hpp>

#include <boost/scope/scope_exit.hpp>

#include <bit>

void vkrndr::destroy(device_t const& device, image_t const& image)
{
    vkDestroyImageView(device, image.view, nullptr);
    vmaDestroyImage(device.allocator, image.image, image.allocation);
}

vkrndr::image_t vkrndr::create_image(device_t const& device,
    image_create_info_t const& create_info)
{
    image_t rv{.format = create_info.format,
        .sample_count = static_cast<VkSampleCountFlags>(create_info.samples),
        .mip_levels = create_info.mip_levels,
        .extent = create_info.extent};

    VkImageCreateInfo image_info{.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = create_info.chain,
        .flags = create_info.image_flags,
        .imageType = create_info.type,
        .format = create_info.format,
        .extent = create_info.extent,
        .mipLevels = create_info.mip_levels,
        .arrayLayers = create_info.array_layers,
        .samples = create_info.samples,
        .tiling = create_info.tiling,
        .usage = create_info.usage,
        .initialLayout = create_info.initial_layout};
    if (!create_info.sharing_queue_families.empty())
    {
        image_info.sharingMode = VK_SHARING_MODE_CONCURRENT;
        image_info.queueFamilyIndexCount =
            count_cast(create_info.sharing_queue_families.size());
        image_info.pQueueFamilyIndices =
            create_info.sharing_queue_families.data();
    }

    VmaAllocationCreateInfo vma_info{};
    vma_info.flags = create_info.allocation_flags;
    vma_info.usage = VMA_MEMORY_USAGE_AUTO;
    if (create_info.required_memory_flags &
        VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT)
    {
        vma_info.usage = VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED;
    }
    else if (create_info.preferred_memory_flags &
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    {
        vma_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    }
    vma_info.requiredFlags = create_info.required_memory_flags;
    vma_info.preferredFlags = create_info.preferred_memory_flags;
    vma_info.memoryTypeBits = create_info.memory_type_bits;
    vma_info.pool = create_info.pool;
    vma_info.priority = create_info.priority;

    check_result(vmaCreateImage(device.allocator,
        &image_info,
        &vma_info,
        &rv.image,
        &rv.allocation,
        nullptr));

    return rv;
}

[[nodiscard]]
VkImageView vkrndr::create_image_view(device_t const& device,
    VkImage const image,
    VkFormat const format,
    VkImageAspectFlags const aspect_flags,
    uint32_t const mip_levels,
    uint32_t const base_mip_level)
{
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = format;
    view_info.subresourceRange.aspectMask = aspect_flags;
    view_info.subresourceRange.baseMipLevel = base_mip_level;
    view_info.subresourceRange.levelCount = mip_levels;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    VkImageView imageView; // NOLINT
    check_result(vkCreateImageView(device, &view_info, nullptr, &imageView));
    return imageView;
}

vkrndr::image_t vkrndr::create_image_and_view(device_t const& device,
    image_create_info_t const& create_info,
    VkImageAspectFlags const aspect_flags)
{
    image_t rv{create_image(device, create_info)};
    boost::scope::scope_exit rollback{[&device, &rv] { destroy(device, rv); }};

    rv.view = create_image_view(device,
        rv.image,
        create_info.format,
        aspect_flags,
        create_info.mip_levels);

    rollback.set_active(false);

    return rv;
}

void vkrndr::object_name(device_t const& device,
    image_t const& image,
    std::string_view const name)
{
    object_name(device,
        VK_OBJECT_TYPE_IMAGE,
        std::bit_cast<uint64_t>(image.image),
        name);
}
