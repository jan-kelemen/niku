#include <material_manager.hpp>

#include <config.hpp>

#include <cppext_container.hpp>
#include <cppext_numeric.hpp>

#include <ngnast_scene_model.hpp>

#include <ngngfx_aircraft_camera.hpp>
#include <ngngfx_perspective_projection.hpp>

#include <ngnwsi_imgui_layer.hpp>

#include <vkrndr_buffer.hpp>
#include <vkrndr_commands.hpp>
#include <vkrndr_debug_utils.hpp>
#include <vkrndr_descriptors.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_error_code.hpp>
#include <vkrndr_execution_port.hpp>
#include <vkrndr_graphics_pipeline_builder.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_memory.hpp>
#include <vkrndr_pipeline.hpp>
#include <vkrndr_pipeline_layout_builder.hpp>
#include <vkrndr_render_pass.hpp>
#include <vkrndr_synchronization.hpp>
#include <vkrndr_utility.hpp>

#include <vkglsl_shader_set.hpp>

#include <boost/scope/defer.hpp>
#include <boost/scope/scope_exit.hpp>

#include <imgui.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <spdlog/spdlog.h>

#include <volk.h>

#include <vulkan/utility/vk_struct_helper.hpp>
#include <vulkan/vk_enum_string_helper.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <expected>
#include <iterator>
#include <limits>
#include <ranges>
#include <system_error>
#include <tuple>
#include <vector>

// IWYU pragma: no_include "vkrndr_sampler.hpp"
// IWYU pragma: no_include <fmt/base.h>
// IWYU pragma: no_include <fmt/format.h>
// IWYU pragma: no_include <memory>
// IWYU pragma: no_include <tuple>

namespace
{
    struct [[nodiscard]] vertex_t final
    {
        glm::vec3 position;
        glm::vec2 uv;
    };

    struct [[nodiscard]] material_preview_data_t final
    {
        glm::mat4 view;
        glm::mat4 projection;
        glm::vec3 position;
    };

    constexpr std::span<VkVertexInputBindingDescription const>
    binding_description()
    {
        static constexpr std::array descriptions{
            VkVertexInputBindingDescription{.binding = 0,
                .stride = sizeof(vertex_t),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX},
        };

        return descriptions;
    }

    constexpr std::span<VkVertexInputAttributeDescription const>
    attribute_description()
    {
        static constexpr std::array descriptions{
            VkVertexInputAttributeDescription{
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(vertex_t, position),
            },
            VkVertexInputAttributeDescription{
                .location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(vertex_t, uv),
            }};

        return descriptions;
    }

    [[nodiscard]] std::expected<VkDescriptorPool, std::error_code>
    create_descriptor_pool(vkrndr::device_t const& device)
    {
        return create_descriptor_pool(device,
            std::to_array<VkDescriptorPoolSize>({
                // clang-format off
                {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1},
                // clang-format on
            }),
            1,
            VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT)
            .transform_error(vkrndr::make_error_code);
    }

    [[nodiscard]] std::expected<VkDescriptorSetLayout, std::error_code>
    create_material_preview_descriptor_layout(vkrndr::device_t const& device)
    {
        static constexpr VkDescriptorSetLayoutBinding position_binding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags =
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        };

        return create_descriptor_set_layout(device,
            cppext::as_span(position_binding))
            .transform_error(vkrndr::make_error_code);
    }

    void update_material_preview_descriptor_set(vkrndr::device_t const& device,
        VkDescriptorSet const descriptor_set,
        vkrndr::buffer_t const& buffer)
    {
        VkDescriptorBufferInfo const buffer_info{
            vkrndr::buffer_descriptor(buffer)};

        VkWriteDescriptorSet const write_info{
            .sType = vku::GetSType<VkWriteDescriptorSet>(),
            .dstSet = descriptor_set,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &buffer_info,
        };

        vkUpdateDescriptorSets(device, 1, &write_info, 0, nullptr);
    }

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

    std::expected<vkrndr::buffer_t, std::error_code> create_sphere_geometry(
        uint32_t const resolution,
        vkrndr::device_t const& device,
        std::function<std::expected<void, std::error_code>(
            std::function<void(VkCommandBuffer)> const&)> const&
            execute_transfer)
    {
        std::vector<vertex_t> vertices;
        std::vector<uint32_t> indices;

        for (uint32_t const i : std::views::iota(uint32_t{0}, resolution + 1))
        {
            float const v{cppext::as_fp(i) / cppext::as_fp(resolution)};
            float const theta{glm::pi<float>() * v};

            float const sin_theta{std::sin(theta)};
            float const cos_theta{std::cos(theta)};

            for (uint32_t const j :
                std::views::iota(uint32_t{0}, resolution + 1))
            {
                float const u{cppext::as_fp(j) / cppext::as_fp(resolution)};
                float const phi{2.0f * glm::pi<float>() * u};

                float const sin_phi{std::sin(phi)};
                float const cos_phi{std::cos(phi)};

                vertices.emplace_back(glm::vec3{sin_theta * cos_phi,
                                          cos_theta,
                                          sin_theta * sin_phi},
                    glm::vec2{u, v});
            }
        }

        for (uint32_t const i : std::views::iota(uint32_t{0}, resolution))
        {
            for (uint32_t const j : std::views::iota(uint32_t{0}, resolution))
            {
                uint32_t const first{i * (resolution + 1) + j};
                uint32_t const second{first + resolution + 1};

                indices.push_back(first);
                indices.push_back(second);
                indices.push_back(first + 1);

                indices.push_back(second);
                indices.push_back(second + 1);
                indices.push_back(first + 1);
            }
        }

        vkrndr::buffer_t const staging{create_staging_buffer(device,
            vertices.size() * sizeof(vertex_t) +
                indices.size() * sizeof(uint32_t))};
        boost::scope::scope_exit const staging_guard{
            [&device, &staging]() { destroy(device, staging); }};
        {
            vkrndr::mapped_memory_t memory{map_memory(device, staging)};

            std::byte* ptr = memory.as<std::byte>();
            for (auto const& vertex : vertices)
            {
                memcpy(ptr,
                    glm::value_ptr(vertex.position),
                    sizeof(vertex.position));
                ptr += sizeof(vertex.position);

                memcpy(ptr, glm::value_ptr(vertex.uv), sizeof(vertex.uv));
                ptr += sizeof(vertex.uv);
            }
            memcpy(ptr, indices.data(), indices.size() * sizeof(uint32_t));

            unmap_memory(device, &memory);
        }

        vkrndr::buffer_t const vertex_index_buffer{create_buffer(device,
            {.size = staging.size,
                .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                    VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .required_memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT})};
        VKRNDR_IF_DEBUG_UTILS(object_name(device,
            vertex_index_buffer,
            "Material Manager sphere geometry"));

        return execute_transfer(
            [&staging, &vertex_index_buffer](VkCommandBuffer cb)
            {
                VkBufferCopy2 const region{
                    .sType = vku::GetSType<VkBufferCopy2>(),
                    .size = staging.size,
                };

                VkCopyBufferInfo2 const info{
                    .sType = vku::GetSType<VkCopyBufferInfo2>(),
                    .srcBuffer = staging,
                    .dstBuffer = vertex_index_buffer,
                    .regionCount = 1,
                    .pRegions = &region,
                };

                vkCmdCopyBuffer2(cb, &info);

                vkrndr::wait_for(cb,
                    {},
                    std::array{vkrndr::on_stage(
                        vkrndr::with_access(
                            vkrndr::buffer_barrier(vertex_index_buffer),
                            VK_ACCESS_2_TRANSFER_WRITE_BIT,
                            VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT |
                                VK_ACCESS_2_INDEX_READ_BIT),
                        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT |
                            VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT)},
                    {});
            })
            .transform([&vertex_index_buffer, &device, &staging]()
                { return vertex_index_buffer; })
            .transform_error(
                [&vertex_index_buffer, &device](std::error_code&& error)
                {
                    destroy(device, vertex_index_buffer);
                    return error;
                });
    }

    std::expected<std::tuple<VkDescriptorSetLayout,
                      VkDescriptorSet,
                      vkrndr::buffer_t,
                      vkrndr::pipeline_layout_t,
                      vkrndr::pipeline_t>,
        std::error_code>
    create_material_preview_shader(vkrndr::device_t const& device,
        VkDescriptorPool pool)
    {
        vkglsl::shader_set_t shaders{editor::enable_shader_debug_symbols,
            editor::enable_shader_optimization};

        std::expected<vkrndr::shader_module_t, std::error_code> const
            add_vertex_result{add_shader_module_from_path(shaders,
                device,
                VK_SHADER_STAGE_VERTEX_BIT,
                "material_preview.vert")};
        if (!add_vertex_result)
        {
            return std::unexpected{add_vertex_result.error()};
        }
        boost::scope::defer_guard destroy_vtx{
            [&device, &shd = add_vertex_result.value()]()
            { destroy(device, shd); }};
        VKRNDR_IF_DEBUG_UTILS(object_name(device,
            *add_vertex_result,
            "Material Preview Vertex Shader"));

        std::expected<vkrndr::shader_module_t, std::error_code> const
            add_fragment_result{add_shader_module_from_path(shaders,
                device,
                VK_SHADER_STAGE_FRAGMENT_BIT,
                "material_preview.frag")};
        if (!add_fragment_result)
        {
            return std::unexpected{add_fragment_result.error()};
        }
        boost::scope::defer_guard destroy_frag{
            [&device, &shd = add_fragment_result.value()]()
            { destroy(device, shd); }};
        VKRNDR_IF_DEBUG_UTILS(object_name(device,
            *add_fragment_result,
            "Material Preview Fragment Shader"));

        std::expected<VkDescriptorSetLayout, std::error_code>
            descriptor_layout_result{descriptor_set_layout(shaders, device, 0)};
        if (!descriptor_layout_result)
        {
            return std::unexpected{descriptor_layout_result.error()};
        }
        boost::scope::scope_exit destroy_descriptor_layout{
            [&device, &layout = descriptor_layout_result.value()]()
            { vkDestroyDescriptorSetLayout(device, layout, nullptr); }};
        VKRNDR_IF_DEBUG_UTILS(object_name(device,
            VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
            vkrndr::handle_cast(*descriptor_layout_result),
            "Material Preview Descriptor Layout"));

        vkrndr::pipeline_layout_t pipeline_layout{
            vkrndr::pipeline_layout_builder_t{device}
                .add_descriptor_set_layout(*descriptor_layout_result)
                .build()};
        boost::scope::scope_exit destroy_pipeline_layout{
            [&device, &layout = pipeline_layout]()
            { destroy(device, layout); }};
        VKRNDR_IF_DEBUG_UTILS(object_name(device,
            pipeline_layout,
            "Material Preview Pipeline Layout"));

        vkrndr::pipeline_t pipeline{
            vkrndr::graphics_pipeline_builder_t{device, pipeline_layout}
                .add_vertex_input(binding_description(),
                    attribute_description())
                .add_shader(as_pipeline_shader(*add_vertex_result))
                .add_shader(as_pipeline_shader(*add_fragment_result))
                .add_color_attachment(VK_FORMAT_R16G16B16A16_SFLOAT)
                .with_culling(VK_CULL_MODE_BACK_BIT,
                    VK_FRONT_FACE_COUNTER_CLOCKWISE)
                .build()};
        VKRNDR_IF_DEBUG_UTILS(
            object_name(device, pipeline, "Material Preview Pipeline"));

        VkDescriptorSet descriptor_set{VK_NULL_HANDLE};
        if (VkResult const result{vkrndr::allocate_descriptor_sets(device,
                pool,
                cppext::as_span(*descriptor_layout_result),
                cppext::as_span(descriptor_set))};
            !vkrndr::is_success_result(result))
        {
            return std::unexpected{vkrndr::make_error_code(result)};
        }
        boost::scope::scope_exit destroy_descriptor_set{
            [&device, &pool, &descriptor_set]
            {
                vkrndr::free_descriptor_sets(device,
                    pool,
                    cppext::as_span(descriptor_set));
            }};

        vkrndr::buffer_t const buffer{create_buffer(device,
            {.size = sizeof(material_preview_data_t),
                .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                .allocation_flags =
                    VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
                .required_memory_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT})};
        VKRNDR_IF_DEBUG_UTILS(object_name(device,
            buffer,
            "Material Preview Data Storage Buffer"));
        {
            ngngfx::aircraft_camera_t c;
            c.set_position({0.0f, 0.0f, 5.f});
            c.update();

            ngngfx::perspective_projection_t p;
            p.set_aspect_ratio(1.0f);
            p.set_near_far_planes({0.01f, 10.0f});
            p.update(c.view_matrix());

            vkrndr::mapped_memory_t memory{map_memory(device, buffer)};
            *memory.as<material_preview_data_t>() = {
                .view = c.view_matrix(),
                .projection = p.projection_matrix(),
                .position = c.position(),
            };
            unmap_memory(device, &memory);
        }
        update_material_preview_descriptor_set(device, descriptor_set, buffer);

        destroy_descriptor_set.set_active(false);
        destroy_descriptor_layout.set_active(false);
        destroy_pipeline_layout.set_active(false);

        return std::make_tuple(*descriptor_layout_result,
            descriptor_set,
            buffer,
            pipeline_layout,
            pipeline);
    }
} // namespace

std::expected<entt::entity, std::error_code> editor::create_material_manager(
    entt::registry& registry,
    vkrndr::device_t const& device,
    std::function<std::expected<void, std::error_code>(
        std::function<void(VkCommandBuffer)> const&)> const& execute_transfer)
{
    std::expected<entt::entity, std::error_code> rv{registry.create()};

    registry.emplace<material_manager_t>(*rv);
    auto& ui{registry.emplace<material_manager_ui_t>(*rv)};
    boost::scope::scope_exit remove_entity{
        [&registry, e = *rv] { registry.destroy(e); }};

    if (std::expected<VkDescriptorPool, std::error_code> result{
            create_descriptor_pool(device)})
    {
        ui.descriptor_pool = *result;
    }
    else
    {
        rv = std::unexpected{result.error()};
        return rv;
    }
    boost::scope::scope_exit remove_pool{[&device, &ui]
        { vkrndr::destroy_descriptor_pool(device, ui.descriptor_pool); }};

    if (std::expected<vkrndr::buffer_t, std::error_code> result{
            create_sphere_geometry(ui.material_preview_geometry_resolution,
                device,
                execute_transfer)})
    {
        ui.material_preview_vertex_index_buffer = *result;
    }
    else
    {
        rv = std::unexpected{result.error()};
        return rv;
    }
    boost::scope::scope_exit remove_geometry{[&device, ui]
        { destroy(device, ui.material_preview_vertex_index_buffer); }};

    if (auto result{create_material_preview_shader(device, ui.descriptor_pool)})
    {
        std::tie(ui.material_preview_descriptor_layout,
            ui.material_preview_descriptor,
            ui.material_preview_storage_buffer,
            ui.material_preview_pipeline_layout,
            ui.material_preview_pipeline) = *std::move(result);
    }
    else
    {
        rv = std::unexpected{result.error()};
        return rv;
    }
    remove_geometry.set_active(false);
    remove_pool.set_active(false);
    remove_entity.set_active(false);

    return rv;
}

void editor::destroy_material_manager(entt::handle manager,
    vkrndr::device_t const& device)
{
    if (material_manager_t const* const materials{
            manager.try_get<material_manager_t>()})
    {
        std::ranges::for_each(materials->samplers,
            [&device](VkSampler const& s)
            { vkDestroySampler(device, s, nullptr); });

        std::ranges::for_each(materials->images,
            [&device](vkrndr::image_t const& image)
            { destroy(device, image); });
    }

    if (material_manager_ui_t const* const ui{
            manager.try_get<material_manager_ui_t>()})
    {
        std::ranges::for_each(ui->material_previews,
            [&device](auto const& image) { destroy(device, image.second); });

        destroy(device, ui->material_preview_vertex_index_buffer);

        destroy(device, ui->material_preview_pipeline);
        destroy(device, ui->material_preview_pipeline_layout);

        vkDestroyDescriptorSetLayout(device,
            ui->material_preview_descriptor_layout,
            nullptr);

        destroy_descriptor_pool(device, ui->descriptor_pool);

        destroy(device, ui->material_preview_storage_buffer);
    }
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

void editor::render_material_previews(entt::handle manager_entity,
    vkrndr::device_t const& device,
    ngnwsi::imgui_layer_t& imgui,
    VkCommandBuffer command_buffer)
{
    auto& ui{manager_entity.get<material_manager_ui_t>()};
    auto& manager{manager_entity.get<material_manager_t>()};

    std::vector<std::pair<editor::material_t const*, vkrndr::image_t>> images;

    for (editor::material_t const& material :
        manager.materials | std::views::drop(ui.material_previews.size()))
    {
        images.emplace_back(&material,
            create_image_and_view(device,
                vkrndr::image_2d_create_info_t{
                    .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                    .extent = {1000, 1000},
                    .tiling = VK_IMAGE_TILING_OPTIMAL,
                    .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                        VK_IMAGE_USAGE_SAMPLED_BIT,
                },
                VK_IMAGE_ASPECT_COLOR_BIT));
    }

    if (images.empty())
    {
        return;
    }

    VkViewport const viewport{.x = 0.0f,
        .y = 0.0f,
        .width = cppext::as_fp(1000),
        .height = cppext::as_fp(1000),
        .minDepth = 0.0f,
        .maxDepth = 1.0f};
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D const scissor{{0, 0},
        vkrndr::to_2d_extent(images.back().second.extent)};
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    vkrndr::bind_pipeline(command_buffer, ui.material_preview_pipeline);

    static constexpr VkDeviceSize zero_offset{};
    vkCmdBindVertexBuffers(command_buffer,
        0,
        1,
        &ui.material_preview_vertex_index_buffer.handle,
        &zero_offset);

    vkCmdBindIndexBuffer(command_buffer,
        ui.material_preview_vertex_index_buffer.handle,
        (ui.material_preview_geometry_resolution + 1) *
            (ui.material_preview_geometry_resolution + 1) *
            sizeof(vertex_t),
        VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(command_buffer,
        ui.material_preview_pipeline.type,
        ui.material_preview_pipeline.layout,
        0,
        1,
        &ui.material_preview_descriptor,
        0,
        nullptr);

    std::vector<VkImageMemoryBarrier2> barriers;
    barriers.reserve(images.size());

    for (auto const& [material, image] : images)
    {
        barriers.push_back(vkrndr::with_access(
            vkrndr::to_layout(
                vkrndr::on_stage(vkrndr::image_barrier(image),
                    VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT),
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
            VK_ACCESS_2_NONE,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT));
    }
    vkrndr::wait_for(command_buffer, {}, {}, barriers);
    barriers.clear();

    for (auto const& [material, image] : images)
    {
        vkrndr::render_pass_t color_pass;
        color_pass.with_color_attachment(VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            image.view,
            VkClearValue{.color = {{1.0f, 1.0f, 1.0f, 1.0f}}});

        vkrndr::render_pass_guard_t guard{color_pass.begin(command_buffer,
            VkRect2D{.offset = {0, 0},
                .extent = vkrndr::to_2d_extent(image.extent)})};

        vkCmdDrawIndexed(command_buffer,
            6 *
                ui.material_preview_geometry_resolution *
                ui.material_preview_geometry_resolution,
            1,
            0,
            0,
            0);

        barriers.push_back(vkrndr::with_access(
            vkrndr::with_layout(
                vkrndr::on_stage(vkrndr::image_barrier(image),
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT),
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT));

        ui.material_previews.emplace_back(imgui.create_texture(image), image);
    }

    vkrndr::wait_for(command_buffer, {}, {}, barriers);
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

        if (ui.displayed_material_index < ui.material_previews.size())
        {
            auto const& [descriptor, image] =
                ui.material_previews[ui.displayed_material_index];

            ImGui::Image(std::bit_cast<ImTextureID>(descriptor),
                ImVec2(cppext::as_fp(image.extent.width),
                    cppext::as_fp(image.extent.height)));
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
            ui.image_descriptors[ui.displayed_texture_index];
        auto const& image = manager.images[image_index];

        ImGui::Image(std::bit_cast<ImTextureID>(descriptor),
            ImVec2(cppext::as_fp(image.extent.width),
                cppext::as_fp(image.extent.height)));
    }
    ImGui::End();
}
