#include <pbr_renderer.hpp>

#include <environment.hpp>

#include <cppext_cycled_buffer.hpp>
#include <cppext_numeric.hpp>
#include <cppext_pragma_warning.hpp>

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

#include <glm/mat4x4.hpp>
#include <glm/matrix.hpp>
#include <glm/vec3.hpp>
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
    constexpr std::array debug_options{"None",
        "Albedo",
        "Normals",
        "Occlusion",
        "Emissive",
        "Metallic",
        "Roughness",
        "Diff (l,n)",
        "F (l,h)",
        "G (l,v,h)",
        "D (h)",
        "Specular"};

    struct [[nodiscard]] push_constants_t final
    {
        uint32_t transform_index;
        uint32_t material_index;
        uint32_t debug;
    };

    struct [[nodiscard]] transform_t final
    {
        glm::mat4 model;
        glm::mat4 model_inverse;
    };

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

    struct [[nodiscard]] draw_traversal_t final
    {
        // NOLINTBEGIN(cppcoreguidelines-avoid-const-or-ref-data-members)
        vkgltf::model_t const* const model;
        transform_t* const transforms;
        VkPipelineLayout const layout;
        VkCommandBuffer const command_buffer;
        uint32_t const debug;
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
                            cppext::narrow<uint32_t>(primitive.material_index),
                        .debug = debug};

                    if (!model->materials.empty())
                    {
                        auto const& material{
                            model->materials[primitive.material_index]};
                        if (material.alpha_mode != alpha_mode)
                        {
                            continue;
                        }

                        switch_pipeline(material.double_sided, alpha_mode);
                    }
                    else if (alpha_mode != vkgltf::alpha_mode_t::opaque)
                    {
                        continue;
                    }
                    else
                    {
                        switch_pipeline(false, alpha_mode);
                    }

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
    , transform_descriptor_set_layout_{create_transform_descriptor_set_layout(
          backend_->device())}
    , frame_data_{backend_->frames_in_flight(), backend_->frames_in_flight()}
{
    for (auto& data : frame_data_.as_span())
    {
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
    }

    vkDestroyDescriptorSetLayout(backend_->device().logical,
        transform_descriptor_set_layout_,
        nullptr);

    destroy(&backend_->device(), &blending_pipeline_);
    destroy(&backend_->device(), &culling_pipeline_);
    destroy(&backend_->device(), &double_sided_pipeline_);
    destroy(&backend_->device(), &model_);
    destroy(&backend_->device(), &fragment_shader_);
    destroy(&backend_->device(), &vertex_shader_);
    destroy(&backend_->device(), &depth_buffer_);
}

VkPipelineLayout gltfviewer::pbr_renderer_t::pipeline_layout() const
{
    if (!model_.vertex_count)
    {
        return VK_NULL_HANDLE;
    }

    return *double_sided_pipeline_.layout;
}

void gltfviewer::pbr_renderer_t::load_model(vkgltf::model_t&& model,
    VkDescriptorSetLayout environment_layout,
    VkDescriptorSetLayout materials_layout)
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
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        data.transform_uniform_map =
            map_memory(backend_->device(), data.transform_uniform);

        bind_transform_descriptor_set(backend_->device(),
            data.transform_descriptor_set,
            vkrndr::buffer_descriptor(data.transform_uniform));
    }

    recreate_pipelines(environment_layout, materials_layout);
}

void gltfviewer::pbr_renderer_t::draw(VkCommandBuffer command_buffer,
    vkrndr::image_t const& color_image)
{
    frame_data_.cycle();

    ImGui::Begin("PBR debug");
    if (ImGui::BeginCombo("Equation", debug_options[debug_], 0))
    {
        uint32_t i{};
        for (auto const& option : debug_options)
        {
            auto const selected{debug_ == i};

            if (ImGui::Selectable(option, selected))
            {
                debug_ = i;
            }

            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }

            ++i;
        }
        ImGui::EndCombo();
    }
    ImGui::End();

    {
        vkrndr::render_pass_t color_render_pass;
        color_render_pass.with_color_attachment(VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            color_image.view,
            VkClearValue{.color = {{1.0f, 0.5f, 0.5f}}});
        color_render_pass.with_depth_attachment(VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            depth_buffer_.view,
            VkClearValue{.depthStencil = {1.0f, 0}});

        [[maybe_unused]] auto guard{color_render_pass.begin(command_buffer,
            {{0, 0}, color_image.extent})};

        if (!model_.vertex_count)
        {
            return;
        }

        std::array const descriptor_sets{frame_data_->transform_descriptor_set};

        vkrndr::bind_pipeline(command_buffer,
            culling_pipeline_,
            2,
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
                    2,
                    descriptor_sets);
                bound = required_pipeline;
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
            .debug = debug_,
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

void gltfviewer::pbr_renderer_t::recreate_pipelines(
    VkDescriptorSetLayout environment_layout,
    VkDescriptorSetLayout materials_layout)
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
                .add_descriptor_set_layout(environment_layout)
                .add_descriptor_set_layout(materials_layout)
                .add_descriptor_set_layout(transform_descriptor_set_layout_)
                .add_push_constants(VkPushConstantRange{
                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT |
                        VK_SHADER_STAGE_FRAGMENT_BIT,
                    .offset = 0,
                    .size = sizeof(push_constants_t)})
                .build(),
            VK_FORMAT_R16G16B16A16_SFLOAT}
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
            VK_FORMAT_R16G16B16A16_SFLOAT}
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
            VK_FORMAT_R16G16B16A16_SFLOAT}
            .add_shader(as_pipeline_shader(vertex_shader_))
            .add_shader(as_pipeline_shader(fragment_shader_))
            .with_primitive_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .with_rasterization_samples(backend_->device().max_msaa_samples)
            .with_depth_test(depth_buffer_.format)
            .add_vertex_input(binding_description(), attribute_descriptions())
            .with_color_blending(color_blending)
            .build();
}
