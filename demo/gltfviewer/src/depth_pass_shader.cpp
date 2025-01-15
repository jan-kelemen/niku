#include <depth_pass_shader.hpp>

#include <config.hpp>
#include <render_graph.hpp>

#include <cppext_container.hpp>

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

gltfviewer::depth_pass_shader_t::depth_pass_shader_t(vkrndr::backend_t& backend)
    : backend_{&backend}
{
}

gltfviewer::depth_pass_shader_t::~depth_pass_shader_t()
{
    destroy(&backend_->device(), &depth_pipeline_);
    destroy(&backend_->device(), &vertex_shader_);
}

VkPipelineLayout gltfviewer::depth_pass_shader_t::pipeline_layout() const
{
    if (depth_pipeline_.pipeline)
    {
        return *depth_pipeline_.layout;
    }

    return VK_NULL_HANDLE;
}

void gltfviewer::depth_pass_shader_t::draw(render_graph_t const& graph,
    VkCommandBuffer command_buffer,
    vkrndr::image_t const& depth_buffer)
{
    vkrndr::command_buffer_scope_t depth_pass_scope{command_buffer, "Depth"};

    vkrndr::bind_pipeline(command_buffer, depth_pipeline_);

    graph.traverse(vkgltf::alpha_mode_t::opaque,
        command_buffer,
        *depth_pipeline_.layout,
        []([[maybe_unused]] vkgltf::alpha_mode_t const mode,
            [[maybe_unused]] bool const double_sided) { });
}

void gltfviewer::depth_pass_shader_t::load(render_graph_t const& graph,
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

    if (depth_pipeline_.pipeline != VK_NULL_HANDLE)
    {
        destroy(&backend_->device(), &depth_pipeline_);
    }

    depth_pipeline_ =
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
