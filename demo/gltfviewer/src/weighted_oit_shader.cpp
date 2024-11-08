#include "vkrndr_utility.hpp"
#include <weighted_oit_shader.hpp>

#include <render_graph.hpp>

#include <vkglsl_shader_set.hpp>

#include <vkgltf_model.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_debug_utils.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_pipeline.hpp>
#include <vkrndr_render_pass.hpp>
#include <vkrndr_shader_module.hpp>

#include <boost/scope/defer.hpp>

#include <tl/expected.hpp>

#include <volk.h>

#include <array>
#include <cassert>
#include <filesystem>
#include <functional>
#include <span>
#include <string_view>
#include <system_error>

// IWYU pragma: no_include <memory>
// IWYU pragma: no_include <optional>
// IWYU pragma: no_include <type_traits>
// IWYU pragma: no_forward_declare VkDescriptorSet_T

gltfviewer::weighted_oit_shader_t::weighted_oit_shader_t(
    vkrndr::backend_t& backend)
    : backend_{&backend}
{
}

gltfviewer::weighted_oit_shader_t::~weighted_oit_shader_t()
{
    destroy(&backend_->device(), &pbr_pipeline_);
    destroy(&backend_->device(), &reveal_image_);
}

VkPipelineLayout gltfviewer::weighted_oit_shader_t::pipeline_layout() const
{
    if (pbr_pipeline_.pipeline)
    {
        return *pbr_pipeline_.layout;
    }

    return VK_NULL_HANDLE;
}

void gltfviewer::weighted_oit_shader_t::draw(render_graph_t const& graph,
    VkCommandBuffer command_buffer,
    vkrndr::image_t const& color_image,
    vkrndr::image_t const& depth_buffer)
{
    [[maybe_unused]] vkrndr::command_buffer_scope_t const cb_scope{
        command_buffer,
        "PBR Transparent"};

    vkrndr::render_pass_t color_render_pass;
    color_render_pass.with_color_attachment(VK_ATTACHMENT_LOAD_OP_LOAD,
        VK_ATTACHMENT_STORE_OP_STORE,
        color_image.view);
    color_render_pass.with_color_attachment(VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        reveal_image_.view,
        VkClearValue{.color = {{0.0f, 0.0f, 0.0f, 1.0f}}});
    color_render_pass.with_depth_attachment(VK_ATTACHMENT_LOAD_OP_LOAD,
        VK_ATTACHMENT_STORE_OP_NONE,
        depth_buffer.view);

    [[maybe_unused]] auto guard{
        color_render_pass.begin(command_buffer, {{0, 0}, color_image.extent})};

    auto const switch_pipeline =
        []([[maybe_unused]] vkgltf::alpha_mode_t const mode,
            [[maybe_unused]] bool const double_sided) {};

    vkrndr::bind_pipeline(command_buffer, pbr_pipeline_);
    vkrndr::command_buffer_scope_t geometry_pass_scope{command_buffer,
        "Geometry"};
    graph.traverse(vkgltf::alpha_mode_t::blend,
        command_buffer,
        *pbr_pipeline_.layout,
        switch_pipeline);
    geometry_pass_scope.close();
}

void gltfviewer::weighted_oit_shader_t::resize(uint32_t const width,
    uint32_t const height)
{
    destroy(&backend_->device(), &reveal_image_);
    reveal_image_ = create_image_and_view(backend_->device(),
        vkrndr::to_extent(width, height),
        1,
        backend_->device().max_msaa_samples,
        VK_FORMAT_R8_UNORM,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT);
}

void gltfviewer::weighted_oit_shader_t::load(render_graph_t const& graph,
    VkDescriptorSetLayout environment_layout,
    VkDescriptorSetLayout materials_layout,
    VkFormat depth_buffer_format)
{
    using namespace std::string_view_literals;

    vkglsl::shader_set_t shader_set{true, false};

    auto vertex_shader{add_shader_module_from_path(shader_set,
        backend_->device(),
        VK_SHADER_STAGE_VERTEX_BIT,
        "pbr.vert")};
    assert(vertex_shader);
    [[maybe_unused]] boost::scope::defer_guard const destroy_vtx{
        [this, shd = &vertex_shader.value()]()
        { destroy(&backend_->device(), shd); }};

    auto fragment_shader{add_shader_module_from_path(shader_set,
        backend_->device(),
        VK_SHADER_STAGE_FRAGMENT_BIT,
        "pbr.frag",
        std::array{"OIT"sv})};
    assert(fragment_shader);
    [[maybe_unused]] boost::scope::defer_guard const destroy_frag{
        [this, shd = &fragment_shader.value()]()
        { destroy(&backend_->device(), shd); }};

    if (pbr_pipeline_.pipeline != VK_NULL_HANDLE)
    {
        destroy(&backend_->device(), &pbr_pipeline_);
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
    pbr_pipeline_ =
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
            .add_shader(as_pipeline_shader(*vertex_shader))
            .add_shader(as_pipeline_shader(*fragment_shader))
            .add_color_attachment(VK_FORMAT_R16G16B16A16_SFLOAT, color_blending)
            .add_color_attachment(VK_FORMAT_R8_UNORM)
            .with_primitive_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .with_rasterization_samples(backend_->device().max_msaa_samples)
            .with_depth_test(depth_buffer_format, VK_COMPARE_OP_LESS, false)
            .add_vertex_input(graph.binding_description(),
                graph.attribute_description())
            .build();

    object_name(backend_->device(), pbr_pipeline_, "Transparent Pipeline");
}
