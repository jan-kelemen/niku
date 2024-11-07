#include <pbr_renderer.hpp>

#include <render_graph.hpp>

#include <cppext_numeric.hpp>

#include <vkgltf_model.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_debug_utils.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_pipeline.hpp>
#include <vkrndr_render_pass.hpp>
#include <vkrndr_shader_module.hpp>

#include <imgui.h>

#include <volk.h>

#include <array>
#include <filesystem>
#include <ranges>
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
        "UV"};

    struct [[nodiscard]] push_constants_t final
    {
        uint32_t transform_index;
        uint32_t material_index;
        uint32_t debug;
        float ibl_factor;
    };

    struct [[nodiscard]] draw_traversal_t final
    {
        // NOLINTBEGIN(cppcoreguidelines-avoid-const-or-ref-data-members)
        vkgltf::model_t const* const model;
        VkPipelineLayout const layout;
        VkCommandBuffer const command_buffer;
        uint32_t const debug;
        float const ibl_factor;
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
                    drawn += draw_node(root, drawn, switch_pipeline);
                }
                // cppcheck-suppress-end useStlAlgorithm
            }
        }

    private:
        [[nodiscard]] uint32_t draw_node(vkgltf::node_t const& node,
            uint32_t const index,
            auto& switch_pipeline)
        {
            uint32_t drawn{0};
            if (node.mesh)
            {
                // cppcheck-suppress-begin useStlAlgorithm
                for (auto const& primitive : node.mesh->primitives)
                {
                    push_constants_t const pc{.transform_index = index,
                        .material_index =
                            cppext::narrow<uint32_t>(primitive.material_index),
                        .debug = debug,
                        .ibl_factor = ibl_factor};

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
                drawn += draw_node(child, index + drawn, switch_pipeline);
            }
            // cppcheck-suppress-end useStlAlgorithm

            return drawn;
        }
    };

} // namespace

gltfviewer::pbr_renderer_t::pbr_renderer_t(vkrndr::backend_t& backend)
    : backend_{&backend}
{
}

gltfviewer::pbr_renderer_t::~pbr_renderer_t()
{
    destroy(&backend_->device(), &blending_pipeline_);
    destroy(&backend_->device(), &culling_pipeline_);
    destroy(&backend_->device(), &double_sided_pipeline_);
    destroy(&backend_->device(), &fragment_shader_);
    destroy(&backend_->device(), &vertex_shader_);
}

VkPipelineLayout gltfviewer::pbr_renderer_t::pipeline_layout() const
{
    if (double_sided_pipeline_.pipeline)
    {
        return *double_sided_pipeline_.layout;
    }

    return VK_NULL_HANDLE;
}

void gltfviewer::pbr_renderer_t::draw(render_graph_t const& graph,
    VkCommandBuffer command_buffer,
    vkrndr::image_t const& color_image,
    vkrndr::image_t const& depth_buffer)
{
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

    ImGui::SliderFloat("IBL factor", &ibl_factor_, 0.0f, 9.0f);

    ImGui::End();

    {
        [[maybe_unused]] vkrndr::command_buffer_scope_t const cb_scope{
            command_buffer,
            "PBR"};

        vkrndr::render_pass_t color_render_pass;
        color_render_pass.with_color_attachment(VK_ATTACHMENT_LOAD_OP_LOAD,
            VK_ATTACHMENT_STORE_OP_STORE,
            color_image.view);
        color_render_pass.with_depth_attachment(VK_ATTACHMENT_LOAD_OP_LOAD,
            VK_ATTACHMENT_STORE_OP_NONE,
            depth_buffer.view);

        [[maybe_unused]] auto guard{color_render_pass.begin(command_buffer,
            {{0, 0}, color_image.extent})};

        if (graph.model().vertex_count == 0)
        {
            return;
        }

        auto switch_pipeline =
            [command_buffer,
                bound = static_cast<vkrndr::pipeline_t*>(nullptr),
                this](bool const double_sided,
                vkgltf::alpha_mode_t const mode) mutable
        {
            auto* required_pipeline{&culling_pipeline_};
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
                vkrndr::bind_pipeline(command_buffer, *required_pipeline);
                bound = required_pipeline;
            }
        };

        vkrndr::command_buffer_scope_t opaque_pass_scope{command_buffer,
            "Opaque"};
        draw_traversal_t traversal{.model = &graph.model(),
            .layout = *double_sided_pipeline_.layout,
            .command_buffer = command_buffer,
            .debug = debug_,
            .ibl_factor = ibl_factor_,
            .alpha_mode = vkgltf::alpha_mode_t::opaque};
        traversal.draw(switch_pipeline);
        opaque_pass_scope.close();

        vkrndr::command_buffer_scope_t mask_pass_scope{command_buffer, "Mask"};
        traversal.alpha_mode = vkgltf::alpha_mode_t::mask;
        traversal.draw(switch_pipeline);
        mask_pass_scope.close();

        vkrndr::command_buffer_scope_t blend_pass_scope{command_buffer,
            "Blend"};
        traversal.alpha_mode = vkgltf::alpha_mode_t::blend;
        traversal.draw(switch_pipeline);
        blend_pass_scope.close();
    }
}

void gltfviewer::pbr_renderer_t::load(render_graph_t const& graph,
    VkDescriptorSetLayout environment_layout,
    VkDescriptorSetLayout materials_layout,
    VkFormat depth_buffer_format)
{
    std::filesystem::path const vertex_path{"pbr.vert.spv"};
    if (auto const wt{last_write_time(vertex_path)}; vertex_write_time_ != wt)
    {
        if (vertex_shader_.handle)
        {
            destroy(&backend_->device(), &vertex_shader_);
        }

        vertex_shader_ = vkrndr::create_shader_module(backend_->device(),
            vertex_path.c_str(),
            VK_SHADER_STAGE_VERTEX_BIT,
            "main");
        object_name(backend_->device(), vertex_shader_, "PBR vertex");
        vertex_write_time_ = wt;
    }

    std::filesystem::path const fragment_path{"pbr.frag.spv"};
    if (auto const wt{last_write_time(fragment_path)};
        fragment_write_time_ != wt)
    {
        if (fragment_shader_.handle)
        {
            destroy(&backend_->device(), &fragment_shader_);
        }

        fragment_shader_ = vkrndr::create_shader_module(backend_->device(),
            fragment_path.c_str(),
            VK_SHADER_STAGE_FRAGMENT_BIT,
            "main");
        object_name(backend_->device(), fragment_shader_, "PBR fragment");
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
                .add_descriptor_set_layout(graph.descriptor_layout())
                .add_push_constants<push_constants_t>(
                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
                .build()}
            .add_shader(as_pipeline_shader(vertex_shader_))
            .add_shader(as_pipeline_shader(fragment_shader_))
            .add_color_attachment(VK_FORMAT_R16G16B16A16_SFLOAT)
            .with_primitive_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .with_rasterization_samples(backend_->device().max_msaa_samples)
            .with_depth_test(depth_buffer_format)
            .add_vertex_input(graph.binding_description(),
                graph.attribute_description())
            .build();
    object_name(backend_->device(),
        double_sided_pipeline_,
        "Double Sided Pipeline");

    if (culling_pipeline_.pipeline != VK_NULL_HANDLE)
    {
        destroy(&backend_->device(), &culling_pipeline_);
    }

    culling_pipeline_ =
        vkrndr::pipeline_builder_t{backend_->device(),
            double_sided_pipeline_.layout}
            .add_shader(as_pipeline_shader(vertex_shader_))
            .add_shader(as_pipeline_shader(fragment_shader_))
            .add_color_attachment(VK_FORMAT_R16G16B16A16_SFLOAT)
            .with_primitive_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .with_rasterization_samples(backend_->device().max_msaa_samples)
            .with_depth_test(depth_buffer_format)
            .add_vertex_input(graph.binding_description(),
                graph.attribute_description())
            .with_culling(VK_CULL_MODE_BACK_BIT,
                VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .build();
    object_name(backend_->device(), culling_pipeline_, "Culling Pipeline");

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
            double_sided_pipeline_.layout}
            .add_shader(as_pipeline_shader(vertex_shader_))
            .add_shader(as_pipeline_shader(fragment_shader_))
            .add_color_attachment(VK_FORMAT_R16G16B16A16_SFLOAT, color_blending)
            .with_primitive_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .with_rasterization_samples(backend_->device().max_msaa_samples)
            .with_depth_test(depth_buffer_format, VK_COMPARE_OP_LESS, false)
            .add_vertex_input(graph.binding_description(),
                graph.attribute_description())
            .build();
    object_name(backend_->device(), blending_pipeline_, "Blending Pipeline");
}
