#include <pbr_shader.hpp>

#include <config.hpp>
#include <scene_graph.hpp>

#include <ngnast_scene_model.hpp>

#include <vkglsl_shader_set.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_debug_utils.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_graphics_pipeline_builder.hpp>
#include <vkrndr_pipeline.hpp>
#include <vkrndr_pipeline_layout_builder.hpp>
#include <vkrndr_shader_module.hpp>

#include <volk.h>

#include <cassert>
#include <filesystem>
#include <functional>
#include <utility>

// IWYU pragma: no_include <expected>
// IWYU pragma: no_include <chrono>
// IWYU pragma: no_include <memory>
// IWYU pragma: no_include <optional>
// IWYU pragma: no_include <type_traits>
// IWYU pragma: no_include <string_view>
// IWYU pragma: no_include <system_error>
// IWYU pragma: no_forward_declare VkDescriptorSet_T

gltfviewer::pbr_shader_t::pbr_shader_t(vkrndr::backend_t& backend)
    : backend_{&backend}
{
}

gltfviewer::pbr_shader_t::~pbr_shader_t()
{
    destroy(backend_->device(), culling_pipeline_);
    destroy(backend_->device(), double_sided_pipeline_);
    destroy(backend_->device(), pipeline_layout_);
    destroy(backend_->device(), fragment_shader_);
    destroy(backend_->device(), vertex_shader_);
}

VkPipelineLayout gltfviewer::pbr_shader_t::pipeline_layout() const
{
    return pipeline_layout_;
}

void gltfviewer::pbr_shader_t::draw(scene_graph_t const& graph,
    VkCommandBuffer command_buffer)
{
    auto switch_pipeline =
        [command_buffer,
            bound = static_cast<vkrndr::pipeline_t*>(nullptr),
            this]([[maybe_unused]] ngnast::alpha_mode_t const mode,
            bool const double_sided) mutable
    {
        auto* const required_pipeline{
            double_sided ? &double_sided_pipeline_ : &culling_pipeline_};
        if (bound != required_pipeline)
        {
            vkrndr::bind_pipeline(command_buffer, *required_pipeline);
            bound = required_pipeline;
        }
    };

    VKRNDR_IF_DEBUG_UTILS(
        [[maybe_unused]] vkrndr::command_buffer_scope_t const color_pass_scope{
            command_buffer,
            "Opaque & Mask"});
    graph.traverse(static_cast<ngnast::alpha_mode_t>(
                       std::to_underlying(ngnast::alpha_mode_t::opaque) |
                       std::to_underlying(ngnast::alpha_mode_t::mask)),
        command_buffer,
        double_sided_pipeline_.layout,
        switch_pipeline);
}

void gltfviewer::pbr_shader_t::load(scene_graph_t const& graph,
    VkDescriptorSetLayout environment_layout,
    VkDescriptorSetLayout materials_layout,
    VkDescriptorSetLayout shadow_layout,
    VkFormat depth_buffer_format)
{
    vkglsl::shader_set_t shader_set{enable_shader_debug_symbols,
        enable_shader_optimization};

    std::filesystem::path const vertex_path{"pbr.vert"};
    if (auto const wt{last_write_time(vertex_path)}; vertex_write_time_ != wt)
    {
        if (vertex_shader_.handle)
        {
            destroy(backend_->device(), vertex_shader_);
        }

        auto vertex_shader{add_shader_module_from_path(shader_set,
            backend_->device(),
            VK_SHADER_STAGE_VERTEX_BIT,
            vertex_path)};
        assert(vertex_shader);

        vertex_shader_ = *vertex_shader;

        VKRNDR_IF_DEBUG_UTILS(
            object_name(backend_->device(), vertex_shader_, "PBR vertex"));
        vertex_write_time_ = wt;
    }

    std::filesystem::path const fragment_path{"pbr.frag"};
    if (auto const wt{last_write_time(fragment_path)};
        fragment_write_time_ != wt)
    {
        if (fragment_shader_.handle)
        {
            destroy(backend_->device(), fragment_shader_);
        }

        auto fragment_shader{add_shader_module_from_path(shader_set,
            backend_->device(),
            VK_SHADER_STAGE_FRAGMENT_BIT,
            fragment_path)};

        fragment_shader_ = *fragment_shader;

        VKRNDR_IF_DEBUG_UTILS(
            object_name(backend_->device(), fragment_shader_, "PBR fragment"));
        fragment_write_time_ = wt;
    }

    destroy(backend_->device(), double_sided_pipeline_);
    destroy(backend_->device(), culling_pipeline_);
    destroy(backend_->device(), pipeline_layout_);

    pipeline_layout_ = vkrndr::pipeline_layout_builder_t{backend_->device()}
                           .add_descriptor_set_layout(environment_layout)
                           .add_descriptor_set_layout(materials_layout)
                           .add_descriptor_set_layout(shadow_layout)
                           .add_push_constants(VkPushConstantRange{
                               .stageFlags = VK_SHADER_STAGE_VERTEX_BIT |
                                   VK_SHADER_STAGE_FRAGMENT_BIT,
                               .offset = 0,
                               .size = 32})
                           .build();

    double_sided_pipeline_ =
        vkrndr::graphics_pipeline_builder_t{backend_->device(),
            pipeline_layout_}
            .add_shader(as_pipeline_shader(vertex_shader_))
            .add_shader(as_pipeline_shader(fragment_shader_))
            .add_color_attachment(VK_FORMAT_R16G16B16A16_SFLOAT)
            .with_primitive_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .with_rasterization_samples(backend_->device().max_msaa_samples)
            .with_depth_test(depth_buffer_format, VK_COMPARE_OP_LESS_OR_EQUAL)
            .build();
    VKRNDR_IF_DEBUG_UTILS(object_name(backend_->device(),
        double_sided_pipeline_,
        "Double Sided Pipeline"));

    culling_pipeline_ =
        vkrndr::graphics_pipeline_builder_t{backend_->device(),
            pipeline_layout_}
            .add_shader(as_pipeline_shader(vertex_shader_))
            .add_shader(as_pipeline_shader(fragment_shader_))
            .add_color_attachment(VK_FORMAT_R16G16B16A16_SFLOAT)
            .with_primitive_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .with_rasterization_samples(backend_->device().max_msaa_samples)
            .with_depth_test(depth_buffer_format, VK_COMPARE_OP_LESS_OR_EQUAL)
            .with_culling(VK_CULL_MODE_BACK_BIT,
                VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .build();
    VKRNDR_IF_DEBUG_UTILS(
        object_name(backend_->device(), culling_pipeline_, "Culling Pipeline"));
}
