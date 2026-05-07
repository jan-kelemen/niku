#include <material_manager.hpp>

#include <cppext_container.hpp>
#include <cppext_numeric.hpp>

#include <ngnast_scene_model.hpp>

#include <vkrndr_buffer.hpp>
#include <vkrndr_commands.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_error_code.hpp>
#include <vkrndr_execution_port.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_memory.hpp>
#include <vkrndr_synchronization.hpp>
#include <vkrndr_utility.hpp>

#include <boost/scope/defer.hpp>
#include <boost/scope/scope_exit.hpp> // for boost::scope::scope_exit

#include <spdlog/spdlog.h>

#include <volk.h>

#include <vulkan/utility/vk_struct_helper.hpp>
#include <vulkan/vk_enum_string_helper.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <expected>
#include <iterator>
#include <limits>
#include <system_error>
#include <vector>

// IWYU pragma: no_include <fmt/base.h>
// IWYU pragma: no_include <fmt/format.h>
// IWYU pragma: no_include <memory>
// IWYU pragma: no_include <tuple>

namespace
{
    [[nodiscard]] VkBufferImageCopy mip_to_buffer_region(
        ngnast::image_mip_level_t const& level,
        uint32_t const mip)
    {
        return {
            .bufferOffset = level.data_offset,
            .imageSubresource =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = mip,
                    .layerCount = 1,
                },
            .imageExtent = {level.extent.width, level.extent.height, 1},
        };
    }
} // namespace

std::expected<editor::material_manager_t, std::error_code>
editor::create_material_manager([[maybe_unused]] vkrndr::device_t const& device)
{
    return {};
}

void editor::destroy(vkrndr::device_t const& device,
    material_manager_t const& materials)
{
    std::ranges::for_each(materials.samplers,
        [&device](VkSampler const& s)
        { vkDestroySampler(device, s, nullptr); });

    std::ranges::for_each(materials.images,
        [&device](vkrndr::image_t const& image) { destroy(device, image); });
}

std::expected<std::vector<vkrndr::image_t>, std::error_code>
editor::transfer_images(vkrndr::device_t const& device,
    vkrndr::execution_port_t& transfer_queue,
    vkrndr::execution_port_t& graphics_queue,
    std::span<ngnast::image_t> const& images)
{
    std::expected<std::vector<vkrndr::image_t>, std::error_code> rv{
        std::unexpected{
            vkrndr::make_error_code(VK_ERROR_INITIALIZATION_FAILED)}};

    std::vector<vkrndr::image_t> gpu_images;
    gpu_images.reserve(images.size());
    boost::scope::scope_exit rollback_gpu_images{[&device, &gpu_images]()
        {
            std::ranges::for_each(gpu_images,
                [&device](auto const& image) { destroy(device, image); });
        }};

    // Calculate staging buffer size equal to the largest image data and create
    // GPU images
    size_t largest_image_size{};

    std::vector<vkrndr::image_t*> images_for_mip_generation;
    images_for_mip_generation.reserve(images.size());

    for (ngnast::image_t const& image : images)
    {
        largest_image_size = std::max(largest_image_size, image.data_size);
        auto const& base_mip{image.mip_levels.front()};

        uint32_t effective_mips_for_image{
            vkrndr::max_mip_levels(base_mip.extent.width,
                base_mip.extent.height)};

        auto properties{vku::InitStruct<VkFormatProperties2>()};
        vkGetPhysicalDeviceFormatProperties2(device, image.format, &properties);
        if (!vkrndr::supports_flags(
                properties.formatProperties.linearTilingFeatures,
                VK_FORMAT_FEATURE_2_BLIT_DST_BIT))
        {
            spdlog::warn(
                "Mipmap generation disabled for image in source format {}",
                string_VkFormat(image.format));

            effective_mips_for_image =
                cppext::narrow<uint32_t>(image.mip_levels.size());
        }

        gpu_images.push_back(create_image_and_view(device,
            vkrndr::image_2d_create_info_t{.format = image.format,
                .extent = image.mip_levels[0].extent,
                .mip_levels = effective_mips_for_image,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                    VK_IMAGE_USAGE_SAMPLED_BIT,
                .required_memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
            VK_IMAGE_ASPECT_COLOR_BIT));

        if (effective_mips_for_image != image.mip_levels.size())
        {
            images_for_mip_generation.push_back(&gpu_images.back());
        }
    }

    vkrndr::buffer_t staging_buffer{
        vkrndr::create_staging_buffer(device, largest_image_size)};
    boost::scope::scope_exit rollback_staging_buffer{
        [&device, &staging_buffer]() { destroy(device, staging_buffer); }};
    vkrndr::mapped_memory_t staging_map{map_memory(device, staging_buffer)};

    VkCommandPool command_pool{VK_NULL_HANDLE};
    if (std::expected<VkCommandPool, std::error_code> const result{
            create_command_pool(device,
                transfer_queue.queue_family(),
                VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                    VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT)})
    {
        command_pool = *result;
    }
    else
    {
        spdlog::error("Failed to create command pool on queue family {}",
            transfer_queue.queue_family());
        rv = std::unexpected{result.error()};
        return rv;
    }
    boost::scope::defer_guard rollback_command_pool{[&device, &command_pool]()
        { destroy_command_pool(device, command_pool); }};

    VkCommandBuffer command_buffer{VK_NULL_HANDLE};
    if (std::expected<void, std::error_code> const result{
            allocate_command_buffers(device,
                command_pool,
                true,
                cppext::as_span(command_buffer))};
        !result)
    {
        spdlog::error("Failed to create command buffer to transfer images");
        rv = std::unexpected{result.error()};
        return rv;
    }

    VkFence const fence{create_fence(device, false)};
    boost::scope::defer_guard rollback_fence{
        [&device, &fence]() { vkDestroyFence(device, fence, nullptr); }};

    // Transfer predefined mips of images
    std::vector<VkBufferImageCopy> buffer_regions;
    for (auto const& [image, gpu_image] : std::views::zip(images, gpu_images))
    {
        memcpy(staging_map.as<std::byte>(), image.data.get(), image.data_size);

        buffer_regions.reserve(image.mip_levels.size());
        std::ranges::transform(image.mip_levels,
            std::back_inserter(buffer_regions),
            [mip = uint32_t{0}](ngnast::image_mip_level_t const& l) mutable
            { return mip_to_buffer_region(l, mip++); });

        VkCommandBufferBeginInfo const begin_info{
            .sType = vku::GetSType<VkCommandBufferBeginInfo>(),
        };

        if (VkResult const result{
                vkBeginCommandBuffer(command_buffer, &begin_info)};
            !vkrndr::is_success_result(result))
        {
            rv = std::unexpected{vkrndr::make_error_code(result)};
            return rv;
        }

        auto const transfer_mips{vkrndr::count_cast(buffer_regions.size())};

        vkrndr::wait_for_transfer_write(gpu_image,
            command_buffer,
            transfer_mips);

        vkCmdCopyBufferToImage(command_buffer,
            staging_buffer,
            gpu_image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            transfer_mips,
            buffer_regions.data());

        if (image.mip_levels.size() == gpu_image.mip_levels)
        {
            wait_for_transfer_write_completed(gpu_image,
                command_buffer,
                transfer_mips);
        }
        else
        {
            auto const barrier{vkrndr::to_layout(
                vkrndr::image_barrier(gpu_image,
                    {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel = 1,
                        .levelCount = gpu_image.mip_levels - 1,
                        .layerCount = 1,
                    }),
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)};

            vkrndr::wait_for(command_buffer, {}, {}, cppext::as_span(barrier));
        }

        if (VkResult const result{vkEndCommandBuffer(command_buffer)};
            !vkrndr::is_success_result(result))
        {
            rv = std::unexpected{vkrndr::make_error_code(result)};
            return rv;
        }

        buffer_regions.clear();

        VkSubmitInfo const submit{
            .sType = vku::GetSType<VkSubmitInfo>(),
            .commandBufferCount = 1,
            .pCommandBuffers = &command_buffer,
        };

        if (VkResult const result{
                transfer_queue.submit(cppext::as_span(submit), fence)};
            !vkrndr::is_success_result(result))
        {
            rv = std::unexpected{vkrndr::make_error_code(result)};
            return rv;
        }

        if (VkResult const result{vkWaitForFences(device,
                1,
                &fence,
                VK_FALSE,
                std::numeric_limits<uint64_t>::max())};
            !vkrndr::is_success_result(result))
        {
            rv = std::unexpected{vkrndr::make_error_code(result)};
            return rv;
        }

        if (VkResult const result{vkResetFences(device, 1, &fence)};
            !vkrndr::is_success_result(result))
        {
            rv = std::unexpected{vkrndr::make_error_code(result)};
            return rv;
        }
    }
    unmap_memory(device, &staging_map);
    destroy(device, staging_buffer);
    rollback_staging_buffer.set_active(false);

    if (!images_for_mip_generation.empty())
    {
        if (transfer_queue.queue_family() != graphics_queue.queue_family())
        {
            if (std::expected<VkCommandPool, std::error_code> const result{
                    create_command_pool(device,
                        graphics_queue.queue_family(),
                        VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT)})
            {
                destroy_command_pool(device, command_pool);
                command_pool = *result;
            }
            else
            {
                spdlog::error(
                    "Failed to create command pool on queue family {}",
                    graphics_queue.queue_family());
                rv = std::unexpected{result.error()};
                return rv;
            }

            if (std::expected<void, std::error_code> const result{
                    allocate_command_buffers(device,
                        command_pool,
                        true,
                        cppext::as_span(command_buffer))};
                !result)
            {
                spdlog::error(
                    "Failed to create command buffer to generate mipmaps");
                rv = std::unexpected{result.error()};
                return rv;
            }
        }

        VkCommandBufferBeginInfo const begin_info{
            .sType = vku::GetSType<VkCommandBufferBeginInfo>(),
        };

        if (VkResult const result{
                vkBeginCommandBuffer(command_buffer, &begin_info)};
            !vkrndr::is_success_result(result))
        {
            rv = std::unexpected{vkrndr::make_error_code(result)};
            return rv;
        }

        for (vkrndr::image_t const* const image : images_for_mip_generation)
        {
            generate_mipmaps(device,
                *image,
                command_buffer,
                image->format,
                vkrndr::to_2d_extent(image->extent),
                image->mip_levels);
        }

        if (VkResult const result{vkEndCommandBuffer(command_buffer)};
            !vkrndr::is_success_result(result))
        {
            rv = std::unexpected{vkrndr::make_error_code(result)};
            return rv;
        }

        VkSubmitInfo const submit{
            .sType = vku::GetSType<VkSubmitInfo>(),
            .commandBufferCount = 1,
            .pCommandBuffers = &command_buffer,
        };

        if (VkResult const result{
                graphics_queue.submit(cppext::as_span(submit), fence)};
            !vkrndr::is_success_result(result))
        {
            rv = std::unexpected{vkrndr::make_error_code(result)};
            return rv;
        }

        if (VkResult const result{vkWaitForFences(device,
                1,
                &fence,
                VK_FALSE,
                std::numeric_limits<uint64_t>::max())};
            !vkrndr::is_success_result(result))
        {
            rv = std::unexpected{vkrndr::make_error_code(result)};
            return rv;
        }
    }

    rv = std::move(gpu_images);
    rollback_gpu_images.set_active(false);

    return rv;
}
