#include <material_manager.hpp>

#include <cppext_container.hpp>
#include <cppext_numeric.hpp>

#include <ngnast_scene_model.hpp>

#include <ngnwsi_imgui_layer.hpp>

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

#include <imgui.h>

#include <glm/gtc/type_ptr.hpp>

#include <spdlog/spdlog.h>

#include <volk.h>

#include <vulkan/utility/vk_struct_helper.hpp>
#include <vulkan/vk_enum_string_helper.h>

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <expected>
#include <iterator>
#include <limits>
#include <system_error>
#include <vector>

// IWYU pragma: no_include "vkrndr_sampler.hpp"
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

    editor::material_t to_gpu_material(ngnast::material_t const& m,
        std::span<editor::texture_t> const& textures,
        std::span<uint32_t> const& texture_remap_table)
    {
        auto const to_private_indices = [&texture_remap_table, &textures](
                                            ngnast::texture_t const& tex)
        {
            assert(tex.texture_index < texture_remap_table.size());
            editor::texture_t const& actual_indices{
                textures[texture_remap_table[tex.texture_index]]};
            return std::make_pair(actual_indices.image_index,
                actual_indices.sampler_index);
        };

        editor::material_t rv{
            .base_color_factor = m.pbr_metallic_roughness.base_color_factor,
            .emissive_factor = m.emissive_factor,
            .alpha_cutoff = m.alpha_mode == ngnast::alpha_mode_t::mask
                ? m.alpha_cutoff
                : 0.0f,
            .metallic_factor = m.pbr_metallic_roughness.metallic_factor,
            .roughness_factor = m.pbr_metallic_roughness.roughness_factor,
            .occlusion_strength = m.occlusion_strength,
            .normal_scale = m.normal_scale,
            .double_sided = static_cast<uint32_t>(m.double_sided),
            .emissive_strength = m.emissive_strength};

        if (auto const* const texture{
                m.pbr_metallic_roughness.base_color_texture})
        {
            std::tie(rv.base_color_texture_index, rv.base_color_sampler_index) =
                to_private_indices(*texture);
        }

        if (auto const* const texture{
                m.pbr_metallic_roughness.metallic_roughness_texture})
        {
            std::tie(rv.metallic_roughness_texture_index,
                rv.metallic_roughness_sampler_index) =
                to_private_indices(*texture);
        }

        if (auto const* const texture{m.normal_texture})
        {
            std::tie(rv.normal_texture_index, rv.normal_sampler_index) =
                to_private_indices(*texture);
        }

        if (auto const* const texture{m.emissive_texture})
        {
            std::tie(rv.emissive_texture_index, rv.emissive_sampler_index) =
                to_private_indices(*texture);
        }

        if (auto const* const texture{m.occlusion_texture})
        {
            std::tie(rv.occlusion_texture_index, rv.occlusion_sampler_index) =
                to_private_indices(*texture);
        }

        return rv;
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

uint32_t editor::add_texture(entt::handle manager_entity,
    uint32_t const sampler_index,
    uint32_t const image_index)
{
    auto& manager{manager_entity.get<material_manager_t>()};
    manager.textures.emplace_back(image_index, sampler_index);

    return cppext::narrow<uint32_t>(manager.textures.size() - 1);
}

uint32_t editor::add_material(entt::handle manager_entity,
    ngnast::material_t const& asset_material,
    std::span<uint32_t> const& texture_remap_table)
{
    auto& manager{manager_entity.get<material_manager_t>()};
    manager.materials.push_back(
        to_gpu_material(asset_material, manager.textures, texture_remap_table));

    return cppext::narrow<uint32_t>(manager.materials.size() - 1);
}

void editor::draw_material_manager(entt::handle manager_entity,
    ngnwsi::imgui_layer_t& imgui)
{
    auto const material_slider =
        [](char const* const name, uint32_t& value, auto upper_limit)
    {
        int const ll{-1};
        int const ul{cppext::narrow<int>(upper_limit)};
        if (int temporary{value != std::numeric_limits<uint32_t>::max()
                    ? cppext::narrow<int>(value)
                    : -1};
            ImGui::SliderInt(name, &temporary, ll, ul))
        {
            if (temporary < 0)
            {
                value = std::numeric_limits<uint32_t>::max();
            }
            else
            {
                value = cppext::narrow<uint32_t>(std::clamp(temporary, 0, ul));
            }

            return true;
        }
        return false;
    };

    auto& ui{manager_entity.get<material_manager_ui_t>()};
    auto& manager{manager_entity.get<material_manager_t>()};

    auto const texture_slider =
        [&manager, &material_slider](char const* const name, uint32_t& value)
    { return material_slider(name, value, manager.textures.size() - 1); };

    auto const sampler_slider =
        [&manager, &material_slider](char const* const name, uint32_t& value)
    { return material_slider(name, value, manager.samplers.size() - 1); };

    for (texture_t const& texture :
        manager.textures | std::views::drop(ui.image_descriptors.size()))
    {
        ui.image_descriptors.emplace_back(texture.image_index,
            imgui.create_texture(manager.images[texture.image_index]));
    }

    ImGui::Begin("Material Manager");
    if (int index{cppext::narrow<int>(ui.displayed_material_index)},
        size{std::max(0, cppext::narrow<int>(manager.materials.size()) - 1)};
        ImGui::SliderInt("Material Index", &index, 0, size))
    {
        ui.displayed_material_index =
            cppext::narrow<size_t>(std::clamp(index, 0, size));
    }

    if (!manager.materials.empty())
    {
        editor::material_t material{
            manager.materials[ui.displayed_material_index]};

        [[maybe_unused]] bool modified{false};
        modified |= ImGui::SliderFloat4("Base Color Factor",
            glm::value_ptr(material.base_color_factor),
            0.0f,
            1.0f);
        modified |= texture_slider("Base Color Texture Index",
            material.base_color_texture_index);
        modified |= sampler_slider("Base Color Sampler Index",
            material.base_color_sampler_index);
        modified |= ImGui::SliderFloat("Alpha Cutoff",
            &material.alpha_cutoff,
            0.0f,
            1.0f);
        modified |= texture_slider("Metallic-Roughness Texture Index",
            material.metallic_roughness_texture_index);
        modified |= sampler_slider("Metallic-Roughness Sampler Index",
            material.metallic_roughness_sampler_index);
        modified |= ImGui::SliderFloat("Metallic Factor",
            &material.metallic_factor,
            0.0f,
            1.0f);
        modified |= ImGui::SliderFloat("Roughness Factor",
            &material.roughness_factor,
            0.0f,
            1.0f);
        modified |= texture_slider("Normal Texture Index",
            material.normal_texture_index);
        modified |= sampler_slider("Normal Sampler Index",
            material.normal_sampler_index);
        modified |= ImGui::SliderFloat("Normal Scale",
            &material.normal_scale,
            0.0f,
            1000.0f);
        modified |= texture_slider("Emissive Texture Index",
            material.emissive_texture_index);
        modified |= sampler_slider("Emissive Sampler Index",
            material.emissive_sampler_index);
        modified |= ImGui::SliderFloat3("Emissive Factor",
            glm::value_ptr(material.emissive_factor),
            0.0f,
            1.0f);
        modified |= ImGui::SliderFloat("Emissive Strength",
            &material.emissive_strength,
            0.0f,
            1000.0f);
        modified |= texture_slider("Occlusion Texture Index",
            material.occlusion_texture_index);
        modified |= sampler_slider("Occlusion Sampler Index",
            material.occlusion_sampler_index);
        modified |= ImGui::SliderFloat("Occlusion Strength",
            &material.occlusion_strength,
            0.0f,
            1.0f);
        if (bool value{static_cast<bool>(material.double_sided)};
            ImGui::Checkbox("Double Sided", &value))
        {
            material.double_sided = static_cast<uint32_t>(value);
            modified |= true;
        }
    }
    ImGui::End();

    ImGui::Begin("Texture Viewer");
    if (int index{cppext::narrow<int>(ui.displayed_texture_index)},
        size{std::max(0, cppext::narrow<int>(ui.image_descriptors.size()) - 1)};
        ImGui::SliderInt("Texture Index", &index, 0, size))
    {
        ui.displayed_texture_index =
            cppext::narrow<size_t>(std::clamp(index, 0, size));
    }

    if (!ui.image_descriptors.empty())
    {
        auto const& [image_index, descriptor] =
            ui.image_descriptors[cppext::narrow<size_t>(
                ui.displayed_texture_index)];
        auto const& image = manager.images[image_index];

        ImGui::Image(std::bit_cast<ImTextureID>(descriptor),
            ImVec2(cppext::as_fp(image.extent.width),
                cppext::as_fp(image.extent.height)));
    }
    ImGui::End();
}
