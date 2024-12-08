#include <gbuffer_shader.hpp>

#include <gbuffer.hpp>
#include <render_graph.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_pipeline.hpp>
#include <vkrndr_render_pass.hpp>
#include <vkrndr_shader_module.hpp>

#include <boost/scope/defer.hpp>

// IWYU pragma: no_include <filesystem>
// IWYU pragma: no_include <memory>
// IWYU pragma: no_include <optional>
// IWYU pragma: no_include <span>

galileo::gbuffer_shader_t::gbuffer_shader_t(vkrndr::backend_t& backend,
    VkDescriptorSetLayout frame_info_layout,
    VkDescriptorSetLayout graph_layout,
    VkFormat const depth_buffer_format)
    : backend_{&backend}
{
    auto vertex_shader{vkrndr::create_shader_module(backend_->device(),
        "gbuffer.vert.spv",
        VK_SHADER_STAGE_VERTEX_BIT,
        "main")};
    [[maybe_unused]] boost::scope::defer_guard const destroy_vert{
        [this, shd = &vertex_shader]() { destroy(&backend_->device(), shd); }};

    auto fragment_shader{vkrndr::create_shader_module(backend_->device(),
        "gbuffer.frag.spv",
        VK_SHADER_STAGE_FRAGMENT_BIT,
        "main")};
    [[maybe_unused]] boost::scope::defer_guard const destroy_frag{
        [this, shd = &fragment_shader]()
        { destroy(&backend_->device(), shd); }};

    pipeline_ = vkrndr::pipeline_builder_t{backend_->device(),
        vkrndr::pipeline_layout_builder_t{backend_->device()}
            .add_descriptor_set_layout(frame_info_layout)
            .add_descriptor_set_layout(graph_layout)
            .build()}
                    .add_shader(as_pipeline_shader(vertex_shader))
                    .add_shader(as_pipeline_shader(fragment_shader))
                    .add_color_attachment(gbuffer_t::position_format)
                    .add_color_attachment(gbuffer_t::normal_format)
                    .add_color_attachment(gbuffer_t::albedo_format)
                    .add_color_attachment(gbuffer_t::specular_format)
                    .with_depth_test(depth_buffer_format)
                    .add_vertex_input(render_graph_t::binding_description(),
                        render_graph_t::attribute_description())
                    .with_culling(VK_CULL_MODE_BACK_BIT,
                        VK_FRONT_FACE_COUNTER_CLOCKWISE)
                    .build();
}

galileo::gbuffer_shader_t::~gbuffer_shader_t()
{
    destroy(&backend_->device(), &pipeline_);
}

VkPipelineLayout galileo::gbuffer_shader_t::pipeline_layout() const
{
    return *pipeline_.layout;
}

void galileo::gbuffer_shader_t::draw(render_graph_t const& graph,
    VkCommandBuffer command_buffer,
    gbuffer_t& gbuffer,
    vkrndr::image_t const& depth_buffer) const
{
    vkrndr::render_pass_t color_pass;
    color_pass.with_color_attachment(VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        gbuffer.position_image().view,
        VkClearValue{.color = {{0.0f, 0.0f, 0.0f, 1.0f}}});
    color_pass.with_color_attachment(VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        gbuffer.normal_image().view,
        VkClearValue{.color = {{0.0f, 0.0f, 0.0f, 1.0f}}});
    color_pass.with_color_attachment(VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        gbuffer.albedo_image().view,
        VkClearValue{.color = {{0.0f, 0.0f, 0.0f, 1.0f}}});
    color_pass.with_color_attachment(VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        gbuffer.specular_image().view,
        VkClearValue{.color = {{0.0f, 0.0f, 0.0f, 1.0f}}});
    color_pass.with_depth_attachment(VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        depth_buffer.view,
        VkClearValue{.depthStencil = {1.0f, 0}});

    [[maybe_unused]] auto const guard{
        color_pass.begin(command_buffer, {{0, 0}, gbuffer.extent()})};

    vkrndr::bind_pipeline(command_buffer, pipeline_);

    graph.draw_node(0, command_buffer);
    graph.draw_node(1, command_buffer);
}
