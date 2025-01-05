#include <pbr_shader.hpp>

#include <config.hpp>
#include <render_graph.hpp>

#include <vkgltf_model.hpp>

#include <vkglsl_shader_set.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_debug_utils.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_pipeline.hpp>
#include <vkrndr_render_pass.hpp>
#include <vkrndr_shader_module.hpp>
#include <vkrndr_synchronization.hpp>

#include <volk.h>

#include <cassert>
#include <filesystem>
#include <functional>
#include <span>
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
    destroy(&backend_->device(), &depth_pipeline_);
    destroy(&backend_->device(), &culling_pipeline_);
    destroy(&backend_->device(), &double_sided_pipeline_);
    destroy(&backend_->device(), &fragment_shader_);
    destroy(&backend_->device(), &vertex_shader_);
}

VkPipelineLayout gltfviewer::pbr_shader_t::pipeline_layout() const
{
    if (double_sided_pipeline_.pipeline)
    {
        return *double_sided_pipeline_.layout;
    }

    return VK_NULL_HANDLE;
}

void gltfviewer::pbr_shader_t::draw(render_graph_t const& graph,
    VkCommandBuffer command_buffer,
    vkrndr::image_t const& color_image,
    vkrndr::image_t const& depth_buffer)
{
    [[maybe_unused]] vkrndr::command_buffer_scope_t const cb_scope{
        command_buffer,
        "PBR"};

    if (!graph.model().vertex_count)
    {
        vkrndr::render_pass_t color_render_pass;
        color_render_pass.with_color_attachment(VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            color_image.view,
            VkClearValue{.color = {{0.0f, 0.0f, 0.0f, 1.0f}}});
        color_render_pass.with_depth_attachment(VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            depth_buffer.view,
            VkClearValue{.depthStencil = {1.0f, 0}});

        [[maybe_unused]] auto guard{color_render_pass.begin(command_buffer,
            {{0, 0}, color_image.extent})};

        return;
    }

    {
        vkrndr::render_pass_t depth_render_pass;
        depth_render_pass.with_depth_attachment(VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            depth_buffer.view,
            VkClearValue{.depthStencil = {1.0f, 0}});

        [[maybe_unused]] auto guard{depth_render_pass.begin(command_buffer,
            {{0, 0}, color_image.extent})};

        vkrndr::bind_pipeline(command_buffer, depth_pipeline_);
        vkrndr::command_buffer_scope_t depth_pass_scope{command_buffer,
            "Depth"};
        graph.traverse(vkgltf::alpha_mode_t::opaque,
            command_buffer,
            *double_sided_pipeline_.layout,
            []([[maybe_unused]] vkgltf::alpha_mode_t const mode,
                [[maybe_unused]] bool const double_sided) { });
        depth_pass_scope.close();
    }

    {
        auto const barrier{vkrndr::with_access(
            vkrndr::on_stage(
                vkrndr::image_barrier(depth_buffer, VK_IMAGE_ASPECT_DEPTH_BIT),
                VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT),
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_2_NONE)};

        vkrndr::wait_for(command_buffer, {}, {}, std::span{&barrier, 1});
    }

    {
        vkrndr::render_pass_t color_render_pass;
        color_render_pass.with_color_attachment(VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            color_image.view,
            VkClearValue{.color = {{0.0f, 0.0f, 0.0f, 1.0f}}});
        color_render_pass.with_depth_attachment(VK_ATTACHMENT_LOAD_OP_LOAD,
            VK_ATTACHMENT_STORE_OP_STORE,
            depth_buffer.view);

        [[maybe_unused]] auto guard{color_render_pass.begin(command_buffer,
            {{0, 0}, color_image.extent})};

        auto switch_pipeline =
            [command_buffer,
                bound = static_cast<vkrndr::pipeline_t*>(nullptr),
                this]([[maybe_unused]] vkgltf::alpha_mode_t const mode,
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

        vkrndr::command_buffer_scope_t color_pass_scope{command_buffer,
            "Opaque & Mask"};
        graph.traverse(static_cast<vkgltf::alpha_mode_t>(
                           std::to_underlying(vkgltf::alpha_mode_t::opaque) |
                           std::to_underlying(vkgltf::alpha_mode_t::mask)),
            command_buffer,
            *double_sided_pipeline_.layout,
            switch_pipeline);
        color_pass_scope.close();
    }
}

void gltfviewer::pbr_shader_t::load(render_graph_t const& graph,
    VkDescriptorSetLayout environment_layout,
    VkDescriptorSetLayout materials_layout,
    VkFormat depth_buffer_format)
{
    vkglsl::shader_set_t shader_set{enable_shader_debug_symbols,
        enable_shader_optimization};

    std::filesystem::path const vertex_path{"pbr.vert"};
    if (auto const wt{last_write_time(vertex_path)}; vertex_write_time_ != wt)
    {
        if (vertex_shader_.handle)
        {
            destroy(&backend_->device(), &vertex_shader_);
        }

        auto vertex_shader{add_shader_module_from_path(shader_set,
            backend_->device(),
            VK_SHADER_STAGE_VERTEX_BIT,
            vertex_path)};
        assert(vertex_shader);

        vertex_shader_ = *vertex_shader;

        object_name(backend_->device(), vertex_shader_, "PBR vertex");
        vertex_write_time_ = wt;
    }

    std::filesystem::path const fragment_path{"pbr.frag"};
    if (auto const wt{last_write_time(fragment_path)};
        fragment_write_time_ != wt)
    {
        if (fragment_shader_.handle)
        {
            destroy(&backend_->device(), &fragment_shader_);
        }

        auto fragment_shader{add_shader_module_from_path(shader_set,
            backend_->device(),
            VK_SHADER_STAGE_FRAGMENT_BIT,
            fragment_path)};

        fragment_shader_ = *fragment_shader;

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
                .add_push_constants(VkPushConstantRange{
                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT |
                        VK_SHADER_STAGE_FRAGMENT_BIT,
                    .offset = 0,
                    .size = 16})
                .build()}
            .add_shader(as_pipeline_shader(vertex_shader_))
            .add_shader(as_pipeline_shader(fragment_shader_))
            .add_color_attachment(VK_FORMAT_R16G16B16A16_SFLOAT)
            .with_primitive_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .with_rasterization_samples(backend_->device().max_msaa_samples)
            .with_depth_test(depth_buffer_format, VK_COMPARE_OP_LESS_OR_EQUAL)
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
            .with_depth_test(depth_buffer_format, VK_COMPARE_OP_LESS_OR_EQUAL)
            .add_vertex_input(graph.binding_description(),
                graph.attribute_description())
            .with_culling(VK_CULL_MODE_BACK_BIT,
                VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .build();
    object_name(backend_->device(), culling_pipeline_, "Culling Pipeline");

    if (depth_pipeline_.pipeline != VK_NULL_HANDLE)
    {
        destroy(&backend_->device(), &depth_pipeline_);
    }

    depth_pipeline_ =
        vkrndr::pipeline_builder_t{backend_->device(),
            double_sided_pipeline_.layout}
            .add_shader(as_pipeline_shader(vertex_shader_))
            .with_primitive_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .with_rasterization_samples(backend_->device().max_msaa_samples)
            .with_depth_test(depth_buffer_format)
            .add_vertex_input(graph.binding_description(),
                graph.attribute_description())
            .with_culling(VK_CULL_MODE_BACK_BIT,
                VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .build();
    object_name(backend_->device(), depth_pipeline_, "Depth Pipeline");
}
