#include <pbr_renderer.hpp>

#include <cppext_cycled_buffer.hpp>
#include <cppext_numeric.hpp>
#include <cppext_pragma_warning.hpp>

#include <niku_camera.hpp>

#include <vkgltf_model.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_buffer.hpp>
#include <vkrndr_depth_buffer.hpp>
#include <vkrndr_descriptors.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_memory.hpp>
#include <vkrndr_pipeline.hpp>
#include <vkrndr_render_pass.hpp>
#include <vkrndr_shader_module.hpp>
#include <vkrndr_utility.hpp>

#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <glm/matrix.hpp>
#include <glm/vec4.hpp>

#include <imgui.h>

#include <volk.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <iterator>
#include <limits>
#include <ranges>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

// IWYU pragma: no_include <chrono>
// IWYU pragma: no_include <memory>
// IWYU pragma: no_include <optional>
// IWYU pragma: no_include <type_traits>
// IWYU pragma: no_include <string_view>
// IWYU pragma: no_forward_declare VkDescriptorSet_T

namespace
{
    struct [[nodiscard]] push_constants_t final
    {
        uint32_t transform_index;
        uint32_t material_index;
    };

    struct [[nodiscard]] transform_t final
    {
        glm::mat4 model;
        glm::mat4 model_inverse;
    };

    DISABLE_WARNING_PUSH
    DISABLE_WARNING_STRUCTURE_WAS_PADDED_DUE_TO_ALIGNMENT_SPECIFIER

    struct [[nodiscard]] alignas(16) material_t final
    {
        glm::vec4 base_color_factor;
        glm::vec3 emmisive_factor;
        uint32_t base_color_texture_index{std::numeric_limits<uint32_t>::max()};
        uint32_t base_color_sampler_index{std::numeric_limits<uint32_t>::max()};
        float alpha_cutoff;
        uint32_t metallic_roughness_texture_index{
            std::numeric_limits<uint32_t>::max()};
        uint32_t metallic_roughness_sampler_index{
            std::numeric_limits<uint32_t>::max()};
        float metallic_factor;
        float roughness_factor;
        float occlusion_strength;
        uint32_t normal_texture_index{std::numeric_limits<uint32_t>::max()};
        uint32_t normal_sampler_index{std::numeric_limits<uint32_t>::max()};
        uint32_t emissive_texture_index{std::numeric_limits<uint32_t>::max()};
        uint32_t emissive_sampler_index{std::numeric_limits<uint32_t>::max()};
        uint32_t occlusion_texture_index{std::numeric_limits<uint32_t>::max()};
        uint32_t occlusion_sampler_index{std::numeric_limits<uint32_t>::max()};
        float normal_scale;
    };

    DISABLE_WARNING_POP

    consteval auto binding_description()
    {
        constexpr std::array descriptions{
            VkVertexInputBindingDescription{.binding = 0,
                .stride = sizeof(vkgltf::vertex_t),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX},
        };

        return descriptions;
    }

    consteval auto attribute_descriptions()
    {
        constexpr std::array descriptions{
            VkVertexInputAttributeDescription{.location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(vkgltf::vertex_t, position)},
            VkVertexInputAttributeDescription{.location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(vkgltf::vertex_t, normal)},
            VkVertexInputAttributeDescription{.location = 2,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                .offset = offsetof(vkgltf::vertex_t, tangent)},
            VkVertexInputAttributeDescription{.location = 3,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                .offset = offsetof(vkgltf::vertex_t, color)},
            VkVertexInputAttributeDescription{.location = 4,
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(vkgltf::vertex_t, uv)},
        };

        return descriptions;
    }

    DISABLE_WARNING_PUSH
    DISABLE_WARNING_STRUCTURE_WAS_PADDED_DUE_TO_ALIGNMENT_SPECIFIER

    struct [[nodiscard]] camera_uniform_t final
    {
        glm::mat4 view;
        glm::mat4 projection;
        alignas(16) glm::vec3 camera_position;
        alignas(
            16) glm::vec3 light_position; // TODO-JK: Remove lights from this
        alignas(16) glm::vec3 light_color;
    };

    DISABLE_WARNING_POP

    [[nodiscard]] VkDescriptorSetLayout create_camera_descriptor_set_layout(
        vkrndr::device_t const& device)
    {
        VkDescriptorSetLayoutBinding camera_uniform_binding{};
        camera_uniform_binding.binding = 0;
        camera_uniform_binding.descriptorType =
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        camera_uniform_binding.descriptorCount = 1;
        camera_uniform_binding.stageFlags =
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array const bindings{camera_uniform_binding};

        VkDescriptorSetLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = vkrndr::count_cast(bindings.size());
        layout_info.pBindings = bindings.data();

        VkDescriptorSetLayout rv; // NOLINT
        vkrndr::check_result(vkCreateDescriptorSetLayout(device.logical,
            &layout_info,
            nullptr,
            &rv));

        return rv;
    }

    [[nodiscard]] VkDescriptorSetLayout create_material_descriptor_set_layout(
        vkrndr::device_t const& device,
        size_t const images,
        size_t const samplers)
    {
        VkDescriptorSetLayoutBinding texture_uniform_binding{};
        texture_uniform_binding.binding = 0;
        texture_uniform_binding.descriptorType =
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        texture_uniform_binding.descriptorCount =
            cppext::narrow<uint32_t>(images);
        texture_uniform_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding sampler_uniform_binding{};
        sampler_uniform_binding.binding = 1;
        sampler_uniform_binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        sampler_uniform_binding.descriptorCount =
            cppext::narrow<uint32_t>(samplers);
        sampler_uniform_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding material_uniform_binding{};
        material_uniform_binding.binding = 2;
        material_uniform_binding.descriptorType =
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        material_uniform_binding.descriptorCount = 1;
        material_uniform_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array const bindings{texture_uniform_binding,
            sampler_uniform_binding,
            material_uniform_binding};

        VkDescriptorSetLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = vkrndr::count_cast(bindings.size());
        layout_info.pBindings = bindings.data();

        VkDescriptorSetLayout rv; // NOLINT
        vkrndr::check_result(vkCreateDescriptorSetLayout(device.logical,
            &layout_info,
            nullptr,
            &rv));

        return rv;
    }

    [[nodiscard]] VkDescriptorSetLayout create_transform_descriptor_set_layout(
        vkrndr::device_t const& device)
    {
        VkDescriptorSetLayoutBinding transform_uniform_binding{};
        transform_uniform_binding.binding = 0;
        transform_uniform_binding.descriptorType =
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        transform_uniform_binding.descriptorCount = 1;
        transform_uniform_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        std::array const bindings{transform_uniform_binding};

        VkDescriptorSetLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = vkrndr::count_cast(bindings.size());
        layout_info.pBindings = bindings.data();

        VkDescriptorSetLayout rv; // NOLINT
        vkrndr::check_result(vkCreateDescriptorSetLayout(device.logical,
            &layout_info,
            nullptr,
            &rv));

        return rv;
    }

    void bind_camera_descriptor_set(vkrndr::device_t const& device,
        VkDescriptorSet const descriptor_set,
        VkDescriptorBufferInfo const camera_uniform_info)
    {
        VkWriteDescriptorSet camera_uniform_write{};
        camera_uniform_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        camera_uniform_write.dstSet = descriptor_set;
        camera_uniform_write.dstBinding = 0;
        camera_uniform_write.dstArrayElement = 0;
        camera_uniform_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        camera_uniform_write.descriptorCount = 1;
        camera_uniform_write.pBufferInfo = &camera_uniform_info;

        std::array const descriptor_writes{camera_uniform_write};

        vkUpdateDescriptorSets(device.logical,
            vkrndr::count_cast(descriptor_writes.size()),
            descriptor_writes.data(),
            0,
            nullptr);
    }

    void bind_material_descriptor_set(vkrndr::device_t const& device,
        VkDescriptorSet const descriptor_set,
        std::span<VkDescriptorImageInfo const> const& textures_info,
        std::span<VkDescriptorImageInfo const> const& samplers_info,
        VkDescriptorBufferInfo const material_info)
    {
        VkWriteDescriptorSet texture_uniform_write{};
        texture_uniform_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        texture_uniform_write.dstSet = descriptor_set;
        texture_uniform_write.dstBinding = 0;
        texture_uniform_write.dstArrayElement = 0;
        texture_uniform_write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        texture_uniform_write.descriptorCount =
            cppext::narrow<uint32_t>(textures_info.size());
        texture_uniform_write.pImageInfo = textures_info.data();

        VkWriteDescriptorSet sampler_uniform_write{};
        sampler_uniform_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        sampler_uniform_write.dstSet = descriptor_set;
        sampler_uniform_write.dstBinding = 1;
        sampler_uniform_write.dstArrayElement = 0;
        sampler_uniform_write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        sampler_uniform_write.descriptorCount =
            cppext::narrow<uint32_t>(samplers_info.size());
        sampler_uniform_write.pImageInfo = samplers_info.data();

        VkWriteDescriptorSet material_uniform_write{};
        material_uniform_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        material_uniform_write.dstSet = descriptor_set;
        material_uniform_write.dstBinding = 2;
        material_uniform_write.dstArrayElement = 0;
        material_uniform_write.descriptorType =
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        material_uniform_write.descriptorCount = 1;
        material_uniform_write.pBufferInfo = &material_info;

        std::array const descriptor_writes{texture_uniform_write,
            sampler_uniform_write,
            material_uniform_write};

        vkUpdateDescriptorSets(device.logical,
            vkrndr::count_cast(descriptor_writes.size()),
            descriptor_writes.data(),
            0,
            nullptr);
    }

    void bind_transform_descriptor_set(vkrndr::device_t const& device,
        VkDescriptorSet const descriptor_set,
        VkDescriptorBufferInfo const transform_uniform_info)
    {
        VkWriteDescriptorSet transform_storage_write{};
        transform_storage_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        transform_storage_write.dstSet = descriptor_set;
        transform_storage_write.dstBinding = 0;
        transform_storage_write.dstArrayElement = 0;
        transform_storage_write.descriptorType =
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        transform_storage_write.descriptorCount = 1;
        transform_storage_write.pBufferInfo = &transform_uniform_info;

        std::array const descriptor_writes{transform_storage_write};

        vkUpdateDescriptorSets(device.logical,
            vkrndr::count_cast(descriptor_writes.size()),
            descriptor_writes.data(),
            0,
            nullptr);
    }

    [[nodiscard]] VkSampler create_sampler(vkrndr::device_t const& device,
        vkgltf::sampler_info_t const& info,
        uint32_t const mip_levels)
    {
        VkPhysicalDeviceProperties properties; // NOLINT
        vkGetPhysicalDeviceProperties(device.physical, &properties);

        VkSamplerCreateInfo sampler_info{};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = info.mag_filter;
        sampler_info.minFilter = info.min_filter;
        sampler_info.addressModeU = info.warp_u;
        sampler_info.addressModeV = info.warp_v;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.anisotropyEnable = VK_TRUE;
        sampler_info.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
        sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        sampler_info.unnormalizedCoordinates = VK_FALSE;
        sampler_info.compareEnable = VK_FALSE;
        sampler_info.compareOp = VK_COMPARE_OP_NEVER;
        sampler_info.mipmapMode = info.mipmap_mode;
        sampler_info.mipLodBias = 0.0f;
        sampler_info.minLod = 0.0f;
        sampler_info.maxLod = cppext::as_fp(mip_levels);

        VkSampler rv; // NOLINT
        vkrndr::check_result(
            vkCreateSampler(device.logical, &sampler_info, nullptr, &rv));

        return rv;
    }

    [[nodiscard]] uint32_t nodes_with_mesh(vkgltf::node_t const& node,
        vkgltf::model_t const& model)
    {
        auto rv{static_cast<uint32_t>(node.mesh != nullptr)};
        // cppcheck-suppress-begin useStlAlgorithm
        for (auto const& child : node.children(model))
        {
            rv += nodes_with_mesh(child, model);
        }
        // cppcheck-suppress-end useStlAlgorithm
        return rv;
    }

    [[nodiscard]] uint32_t required_transforms(vkgltf::model_t const& model)
    {
        uint32_t rv{};
        // cppcheck-suppress-begin useStlAlgorithm
        for (auto const& graph : model.scenes)
        {
            for (auto const& root : graph.roots(model))
            {
                rv += nodes_with_mesh(root, model);
            }
        }
        // cppcheck-suppress-end useStlAlgorithm
        return rv;
    }

    struct [[nodiscard]] materials_transfer_t final
    {
        vkrndr::buffer_t buffer;
        std::vector<VkSampler> samplers;
        VkDescriptorSetLayout descriptor_layout{VK_NULL_HANDLE};
        VkDescriptorSet descriptor_set{VK_NULL_HANDLE};
    };

    materials_transfer_t transfer_materials(vkrndr::backend_t& backend,
        vkgltf::model_t const& model)
    {
        auto const image_and_sampler = [](vkgltf::texture_t const& texture)
        {
            return std::make_pair(cppext::narrow<uint32_t>(texture.image_index),
                cppext::narrow<uint32_t>(texture.sampler_index));
        };

        auto const to_gpu_material = [&image_and_sampler](
                                         vkgltf::material_t const& m)
        {
            DISABLE_WARNING_PUSH
            DISABLE_WARNING_MISSING_FIELD_INITIALIZERS

            material_t rv{
                .base_color_factor = m.pbr_metallic_roughness.base_color_factor,
                .emmisive_factor = m.emmisive_factor,
                .alpha_cutoff = m.alpha_mode == vkgltf::alpha_mode_t::mask
                    ? m.alpha_cutoff
                    : 0.0f,
                .metallic_factor = m.pbr_metallic_roughness.metallic_factor,
                .roughness_factor = m.pbr_metallic_roughness.roughness_factor,
                .occlusion_strength = m.occlusion_strength,
                .normal_scale = m.normal_scale};

            DISABLE_WARNING_POP

            if (auto const* const texture{
                    m.pbr_metallic_roughness.base_color_texture})
            {
                std::tie(rv.base_color_texture_index,
                    rv.base_color_sampler_index) = image_and_sampler(*texture);
            }

            if (auto const* const texture{
                    m.pbr_metallic_roughness.metallic_roughness_texture})
            {
                std::tie(rv.metallic_roughness_texture_index,
                    rv.metallic_roughness_sampler_index) =
                    image_and_sampler(*texture);
            }

            if (auto const* const texture{m.normal_texture})
            {
                std::tie(rv.normal_texture_index, rv.normal_sampler_index) =
                    image_and_sampler(*texture);
            }

            if (auto const* const texture{m.emmisive_texture})
            {
                std::tie(rv.emissive_texture_index, rv.emissive_sampler_index) =
                    image_and_sampler(*texture);
            }

            if (auto const* const texture{m.occlusion_texture})
            {
                std::tie(rv.occlusion_texture_index,
                    rv.occlusion_sampler_index) = image_and_sampler(*texture);
            }

            return rv;
        };

        materials_transfer_t rv;
        {
            vkrndr::buffer_t staging_buffer{
                create_staging_buffer(backend.device(),
                    sizeof(material_t) * model.materials.size())};
            vkrndr::mapped_memory_t staging_buffer_map{
                map_memory(backend.device(), staging_buffer)};
            material_t* const materials{staging_buffer_map.as<material_t>()};

            std::ranges::transform(model.materials, materials, to_gpu_material);

            rv.buffer = create_buffer(backend.device(),
                staging_buffer.size,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            unmap_memory(backend.device(), &staging_buffer_map);

            backend.transfer_buffer(staging_buffer, rv.buffer);

            destroy(&backend.device(), &staging_buffer);
        }

        // clang-format off
        uint32_t const max_mip_levels{std::ranges::max(model.images,
            std::less{},
            &vkrndr::image_t::mip_levels).mip_levels};
        // clang-format on

        for (auto const& sampler : model.samplers)
        {
            rv.samplers.push_back(
                create_sampler(backend.device(), sampler, max_mip_levels));
        }

        rv.descriptor_layout =
            create_material_descriptor_set_layout(backend.device(),
                model.images.size(),
                model.samplers.size());

        vkrndr::create_descriptor_sets(backend.device(),
            backend.descriptor_pool(),
            std::span{&rv.descriptor_layout, 1},
            std::span{&rv.descriptor_set, 1});

        std::vector<VkDescriptorImageInfo> image_descriptors;
        image_descriptors.reserve(model.images.size());
        std::ranges::transform(model.images,
            std::back_inserter(image_descriptors),
            [](vkrndr::image_t const& image)
            { return vkrndr::sampled_image_descriptor(image); });

        std::vector<VkDescriptorImageInfo> sampler_descriptors;
        sampler_descriptors.reserve(model.samplers.size());
        std::ranges::transform(rv.samplers,
            std::back_inserter(sampler_descriptors),
            vkrndr::sampler_descriptor);

        bind_material_descriptor_set(backend.device(),
            rv.descriptor_set,
            image_descriptors,
            sampler_descriptors,
            vkrndr::buffer_descriptor(rv.buffer));

        return rv;
    }

    
    struct [[nodiscard]] draw_traversal_t final
    {
        // NOLINTBEGIN(cppcoreguidelines-avoid-const-or-ref-data-members)
        vkgltf::model_t const* const model;
        transform_t* const transforms;
        VkPipelineLayout const layout;
        VkCommandBuffer const command_buffer;
        // NOLINTEND(cppcoreguidelines-avoid-const-or-ref-data-members)

        vkgltf::alpha_mode_t alpha_mode;

        void draw(auto& switch_pipeline)
        {
            uint32_t drawn{0};
            for (auto const& graph : model->scenes)
            {
                // cppcheck-suppress-begin useStlAlgorithm
                for (auto const& root : graph.roots(*model))
                {
                    drawn += draw_node(root,
                        glm::mat4{1.0f},
                        drawn,
                        switch_pipeline);
                }
                // cppcheck-suppress-end useStlAlgorithm
            }
        }

    private:
        [[nodiscard]] uint32_t draw_node(vkgltf::node_t const& node,
            glm::mat4 const& transform,
            uint32_t const index,
            auto& switch_pipeline)
        {
            auto const node_transform{transform * node.matrix};

            uint32_t drawn{0};
            if (node.mesh)
            {
                transforms[index].model = node_transform;
                transforms[index].model_inverse =
                    glm::transpose(glm::inverse(node_transform));

                // cppcheck-suppress-begin useStlAlgorithm
                for (auto const& primitive : node.mesh->primitives)
                {
                    push_constants_t const pc{.transform_index = index,
                        .material_index =
                            cppext::narrow<uint32_t>(primitive.material_index)};

                    auto const& material{
                        model->materials[primitive.material_index]};
                    if (material.alpha_mode != alpha_mode)
                    {
                        continue;
                    }

                    switch_pipeline(material.double_sided, alpha_mode);

                    vkCmdPushConstants(command_buffer,
                        layout,
                        VK_SHADER_STAGE_VERTEX_BIT |
                            VK_SHADER_STAGE_FRAGMENT_BIT,
                        0,
                        sizeof(pc),
                        &pc);
                    if (primitive.is_indexed)
                    {
                        vkCmdDrawIndexed(command_buffer,
                            primitive.count,
                            1,
                            primitive.first,
                            primitive.vertex_offset,
                            0);
                    }
                    else
                    {
                        vkCmdDraw(command_buffer,
                            primitive.count,
                            1,
                            primitive.first,
                            0);
                    }
                }
                // cppcheck-suppress-end useStlAlgorithm

                ++drawn;
            }

            // cppcheck-suppress-begin useStlAlgorithm
            for (auto const& child : node.children(*model))
            {
                drawn += draw_node(child,
                    node_transform,
                    index + drawn,
                    switch_pipeline);
            }
            // cppcheck-suppress-end useStlAlgorithm

            return drawn;
        }
    };

} // namespace

gltfviewer::pbr_renderer_t::pbr_renderer_t(vkrndr::backend_t& backend)
    : backend_{&backend}
    , depth_buffer_{vkrndr::create_depth_buffer(backend_->device(),
          backend_->extent(),
          false,
          backend_->device().max_msaa_samples)}
    , camera_descriptor_set_layout_{create_camera_descriptor_set_layout(
          backend_->device())}
    , transform_descriptor_set_layout_{create_transform_descriptor_set_layout(
          backend_->device())}
    , frame_data_{backend_->frames_in_flight(), backend_->frames_in_flight()}
{
    for (auto& data : frame_data_.as_span())
    {
        auto const camera_uniform_buffer_size{sizeof(camera_uniform_t)};
        data.camera_uniform = create_buffer(backend_->device(),
            camera_uniform_buffer_size,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        data.camera_uniform_map =
            vkrndr::map_memory(backend_->device(), data.camera_uniform);

        vkrndr::create_descriptor_sets(backend_->device(),
            backend_->descriptor_pool(),
            std::span{&camera_descriptor_set_layout_, 1},
            std::span{&data.camera_descriptor_set, 1});

        bind_camera_descriptor_set(backend_->device(),
            data.camera_descriptor_set,
            vkrndr::buffer_descriptor(data.camera_uniform));

        vkrndr::create_descriptor_sets(backend_->device(),
            backend_->descriptor_pool(),
            std::span{&transform_descriptor_set_layout_, 1},
            std::span{&data.transform_descriptor_set, 1});
    }
}

gltfviewer::pbr_renderer_t::~pbr_renderer_t()
{
    for (auto& data : frame_data_.as_span())
    {
        vkrndr::free_descriptor_sets(backend_->device(),
            backend_->descriptor_pool(),
            std::span{&data.transform_descriptor_set, 1});

        if (data.transform_uniform_map.allocation != VK_NULL_HANDLE)
        {
            unmap_memory(backend_->device(), &data.transform_uniform_map);
            destroy(&backend_->device(), &data.transform_uniform);
        }

        vkrndr::free_descriptor_sets(backend_->device(),
            backend_->descriptor_pool(),
            std::span{&data.camera_descriptor_set, 1});

        unmap_memory(backend_->device(), &data.camera_uniform_map);
        destroy(&backend_->device(), &data.camera_uniform);
    }

    destroy(&backend_->device(), &material_uniform_);
    for (VkSampler sampler : samplers_)
    {
        vkDestroySampler(backend_->device().logical, sampler, nullptr);
    }

    vkrndr::free_descriptor_sets(backend_->device(),
        backend_->descriptor_pool(),
        std::span{&material_descriptor_set_, 1});

    vkDestroyDescriptorSetLayout(backend_->device().logical,
        transform_descriptor_set_layout_,
        nullptr);

    vkDestroyDescriptorSetLayout(backend_->device().logical,
        material_descriptor_set_layout_,
        nullptr);

    vkDestroyDescriptorSetLayout(backend_->device().logical,
        camera_descriptor_set_layout_,
        nullptr);

    destroy(&backend_->device(), &blending_pipeline_);
    destroy(&backend_->device(), &culling_pipeline_);
    destroy(&backend_->device(), &double_sided_pipeline_);
    destroy(&backend_->device(), &model_);
    destroy(&backend_->device(), &fragment_shader_);
    destroy(&backend_->device(), &vertex_shader_);
    destroy(&backend_->device(), &depth_buffer_);
}

void gltfviewer::pbr_renderer_t::load_model(vkgltf::model_t&& model)
{
    auto real_vertex_buffer{create_buffer(backend_->device(),
        model.vertex_buffer.size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};

    std::optional<vkrndr::buffer_t> real_index_buffer;
    if (model.index_buffer.size > 0)
    {
        real_index_buffer = create_buffer(backend_->device(),
            model.index_buffer.size,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }

    backend_->transfer_buffer(model.vertex_buffer, real_vertex_buffer);
    if (real_index_buffer)
    {
        backend_->transfer_buffer(model.index_buffer, *real_index_buffer);
    }

    destroy(&backend_->device(), &model.vertex_buffer);
    destroy(&backend_->device(), &model.index_buffer);
    model.vertex_buffer = real_vertex_buffer;
    model.index_buffer = real_index_buffer.value_or(vkrndr::buffer_t{});

    destroy(&backend_->device(), &model_);

    materials_transfer_t materials_transfer{
        transfer_materials(*backend_, model)};

    for (VkSampler sampler : samplers_)
    {
        vkDestroySampler(backend_->device().logical, sampler, nullptr);
    }
    samplers_ = std::move(materials_transfer.samplers);

    if (material_descriptor_set_ != VK_NULL_HANDLE)
    {
        vkrndr::free_descriptor_sets(backend_->device(),
            backend_->descriptor_pool(),
            std::span{&material_descriptor_set_, 1});
    }
    if (material_descriptor_set_layout_ != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(backend_->device().logical,
            material_descriptor_set_layout_,
            nullptr);
    }
    material_descriptor_set_layout_ = materials_transfer.descriptor_layout;
    material_descriptor_set_ = materials_transfer.descriptor_set;

    if (material_uniform_.allocation != VK_NULL_HANDLE)
    {
        destroy(&backend_->device(), &material_uniform_);
    }
    material_uniform_ = materials_transfer.buffer;

    model_ = std::move(model);

    uint32_t const transform_count{required_transforms(model_)};
    VkDeviceSize const transform_buffer_size{
        transform_count * sizeof(transform_t)};
    for (auto& data : frame_data_.as_span())
    {
        if (data.transform_uniform_map.allocation != VK_NULL_HANDLE)
        {
            unmap_memory(backend_->device(), &data.transform_uniform_map);
            destroy(&backend_->device(), &data.transform_uniform);
        }

        data.transform_uniform = create_buffer(backend_->device(),
            transform_buffer_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        data.transform_uniform_map =
            map_memory(backend_->device(), data.transform_uniform);

        bind_transform_descriptor_set(backend_->device(),
            data.transform_descriptor_set,
            vkrndr::buffer_descriptor(data.transform_uniform));
    }

    recreate_pipelines();
}

void gltfviewer::pbr_renderer_t::update(niku::camera_t const& camera)
{
    frame_data_.cycle();

    // TODO-JK: TEMP
    ImGui::Begin("Light");
    ImGui::SliderFloat3("Position",
        glm::value_ptr(light_position_),
        -500.0f,
        500.0f);
    ImGui::SliderFloat3("Color", glm::value_ptr(light_color_), 0.0f, 1.0f);
    ImGui::End();

    auto* const camera_uniform{
        frame_data_->camera_uniform_map.as<camera_uniform_t>()};
    camera_uniform->view = camera.view_matrix();
    camera_uniform->projection = camera.projection_matrix();
    camera_uniform->camera_position = camera.position();
    camera_uniform->light_position = light_position_;
    camera_uniform->light_color = light_color_;
}

void gltfviewer::pbr_renderer_t::draw(VkCommandBuffer command_buffer,
    vkrndr::image_t const& color_image)
{
    {
        vkrndr::render_pass_t color_render_pass;
        color_render_pass.with_color_attachment(VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            color_image.view,
            VkClearValue{.color = {{1.0f, 0.5f, 0.5f}}});
        color_render_pass.with_depth_attachment(VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            depth_buffer_.view,
            VkClearValue{.depthStencil = {1.0f, 0}});

        [[maybe_unused]] auto guard{color_render_pass.begin(command_buffer,
            {{0, 0}, color_image.extent})};

        if (!model_.vertex_count)
        {
            return;
        }

        std::array const descriptor_sets{frame_data_->camera_descriptor_set,
            material_descriptor_set_,
            frame_data_->transform_descriptor_set};

        vkrndr::bind_pipeline(command_buffer,
            culling_pipeline_,
            0,
            descriptor_sets);

        auto switch_pipeline = [command_buffer,
                                   bound = &culling_pipeline_,
                                   &descriptor_sets,
                                   this](bool const double_sided,
                                   vkgltf::alpha_mode_t const mode) mutable
        {
            auto* required_pipeline{bound};
            if (mode == vkgltf::alpha_mode_t::blend)
            {
                required_pipeline = &blending_pipeline_;
            }
            else if (double_sided)
            {
                required_pipeline = &double_sided_pipeline_;
            }

            if (bound != required_pipeline)
            {
                vkrndr::bind_pipeline(command_buffer,
                    *required_pipeline,
                    0,
                    descriptor_sets);
            }
        };

        VkDeviceSize const zero_offset{};
        vkCmdBindVertexBuffers(command_buffer,
            0,
            1,
            &model_.vertex_buffer.buffer,
            &zero_offset);

        if (model_.index_buffer.buffer != VK_NULL_HANDLE)
        {
            vkCmdBindIndexBuffer(command_buffer,
                model_.index_buffer.buffer,
                0,
                VK_INDEX_TYPE_UINT32);
        }

        draw_traversal_t traversal{.model = &model_,
            .transforms = frame_data_->transform_uniform_map.as<transform_t>(),
            .layout = *double_sided_pipeline_.layout,
            .command_buffer = command_buffer,
            .alpha_mode = vkgltf::alpha_mode_t::opaque};
        traversal.draw(switch_pipeline);

        traversal.alpha_mode = vkgltf::alpha_mode_t::mask;
        traversal.draw(switch_pipeline);

        traversal.alpha_mode = vkgltf::alpha_mode_t::blend;
        traversal.draw(switch_pipeline);
    }
}

void gltfviewer::pbr_renderer_t::resize(uint32_t width, uint32_t height)
{
    destroy(&backend_->device(), &depth_buffer_);
    depth_buffer_ = vkrndr::create_depth_buffer(backend_->device(),
        VkExtent2D{width, height},
        false,
        backend_->device().max_msaa_samples);
}

void gltfviewer::pbr_renderer_t::recreate_pipelines()
{
    std::filesystem::path const vertex_path{"pbr.vert.spv"};
    if (auto const wt{last_write_time(vertex_path)}; vertex_write_time_ != wt)
    {
        vertex_shader_ = vkrndr::create_shader_module(backend_->device(),
            "pbr.vert.spv",
            VK_SHADER_STAGE_VERTEX_BIT,
            "main");
        vertex_write_time_ = wt;
    }

    std::filesystem::path const fragment_path{"pbr.frag.spv"};
    if (auto const wt{last_write_time(fragment_path)};
        fragment_write_time_ != wt)
    {
        fragment_shader_ = vkrndr::create_shader_module(backend_->device(),
            "pbr.frag.spv",
            VK_SHADER_STAGE_FRAGMENT_BIT,
            "main");
        fragment_write_time_ = wt;
    }

    if (double_sided_pipeline_.pipeline != VK_NULL_HANDLE)
    {
        destroy(&backend_->device(), &double_sided_pipeline_);
    }

    double_sided_pipeline_ =
        vkrndr::pipeline_builder_t{backend_->device(),
            vkrndr::pipeline_layout_builder_t{backend_->device()}
                .add_descriptor_set_layout(camera_descriptor_set_layout_)
                .add_descriptor_set_layout(material_descriptor_set_layout_)
                .add_descriptor_set_layout(transform_descriptor_set_layout_)
                .add_push_constants(VkPushConstantRange{
                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT |
                        VK_SHADER_STAGE_FRAGMENT_BIT,
                    .offset = 0,
                    .size = sizeof(push_constants_t)})
                .build(),
            VK_FORMAT_R32G32B32A32_SFLOAT}
            .add_shader(as_pipeline_shader(vertex_shader_))
            .add_shader(as_pipeline_shader(fragment_shader_))
            .with_primitive_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .with_rasterization_samples(backend_->device().max_msaa_samples)
            .with_depth_test(depth_buffer_.format)
            .add_vertex_input(binding_description(), attribute_descriptions())
            .build();

    if (culling_pipeline_.pipeline != VK_NULL_HANDLE)
    {
        destroy(&backend_->device(), &culling_pipeline_);
    }

    culling_pipeline_ =
        vkrndr::pipeline_builder_t{backend_->device(),
            double_sided_pipeline_.layout,
            VK_FORMAT_R32G32B32A32_SFLOAT}
            .add_shader(as_pipeline_shader(vertex_shader_))
            .add_shader(as_pipeline_shader(fragment_shader_))
            .with_primitive_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .with_rasterization_samples(backend_->device().max_msaa_samples)
            .with_depth_test(depth_buffer_.format)
            .add_vertex_input(binding_description(), attribute_descriptions())
            .with_culling(VK_CULL_MODE_BACK_BIT,
                VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .build();

    if (blending_pipeline_.pipeline != VK_NULL_HANDLE)
    {
        destroy(&backend_->device(), &blending_pipeline_);
    }

    VkPipelineColorBlendAttachmentState color_blending{};
    color_blending.blendEnable = VK_TRUE;
    color_blending.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;
    color_blending.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blending.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blending.colorBlendOp = VK_BLEND_OP_ADD;
    color_blending.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blending.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blending.alphaBlendOp = VK_BLEND_OP_ADD;
    blending_pipeline_ =
        vkrndr::pipeline_builder_t{backend_->device(),
            double_sided_pipeline_.layout,
            VK_FORMAT_R32G32B32A32_SFLOAT}
            .add_shader(as_pipeline_shader(vertex_shader_))
            .add_shader(as_pipeline_shader(fragment_shader_))
            .with_primitive_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .with_rasterization_samples(backend_->device().max_msaa_samples)
            .with_depth_test(depth_buffer_.format)
            .add_vertex_input(binding_description(), attribute_descriptions())
            .with_color_blending(color_blending)
            .build();
}
