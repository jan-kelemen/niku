#include <ngngfx_gbuffer.hpp>

#include <vkrndr_image.hpp>

#include <utility>

ngngfx::gbuffer_t ngngfx::create_gbuffer(vkrndr::device_t const& device,
    gbuffer_create_info_t const& create_info,
    VkImageAspectFlags const aspect_flags)
{
    std::vector<vkrndr::image_t> images;
    images.reserve(create_info.formats.size());

    vkrndr::image_2d_create_info_t individual_create_info{
        .chain = create_info.chain,
        .image_flags = create_info.image_flags,
        .extent = create_info.extent,
        .mip_levels = create_info.mip_levels,
        .array_layers = create_info.array_layers,
        .samples = create_info.samples,
        .tiling = create_info.tiling,
        .usage = create_info.usage,
        .sharing_queue_families = create_info.sharing_queue_families,
        .initial_layout = create_info.initial_layout,
        .allocation_flags = create_info.allocation_flags,
        .required_memory_flags = create_info.required_memory_flags,
        .preferred_memory_flags = create_info.preferred_memory_flags,
        .memory_type_bits = create_info.memory_type_bits,
        .pool = create_info.pool,
        .priority = create_info.priority};
    // cppcheck-suppress-begin useStlAlgorithm
    for (auto const format : create_info.formats)
    {
        individual_create_info.format = format;
        images.push_back(vkrndr::create_image_and_view(device,
            individual_create_info,
            aspect_flags));
    }
    // cppcheck-suppress-end useStlAlgorithm

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
