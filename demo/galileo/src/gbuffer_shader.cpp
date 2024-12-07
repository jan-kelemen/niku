#include <gbuffer_shader.hpp>

#include <gbuffer.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_buffer.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_memory.hpp>
#include <vkrndr_pipeline.hpp>
#include <vkrndr_render_pass.hpp>
#include <vkrndr_shader_module.hpp>

#include <boost/scope/defer.hpp>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

// IWYU pragma: no_include <filesystem>
// IWYU pragma: no_include <memory>
// IWYU pragma: no_include <optional>
// IWYU pragma: no_include <span>

namespace
{
    struct [[nodiscard]] line_vertex_t final
    {
        glm::vec3 position;
        char padding;
        glm::vec4 color;
    };

    consteval auto binding_description()
    {
        constexpr std::array descriptions{
            VkVertexInputBindingDescription{.binding = 0,
                .stride = sizeof(line_vertex_t),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX},
        };

        return descriptions;
    }

    consteval auto attribute_description()
    {
        constexpr std::array descriptions{
            VkVertexInputAttributeDescription{.location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(line_vertex_t, position)},
            VkVertexInputAttributeDescription{.location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                .offset = offsetof(line_vertex_t, color)},
        };

        return descriptions;
    }
} // namespace

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

    pipeline_ =
        vkrndr::pipeline_builder_t{backend_->device(),
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
            .add_vertex_input(binding_description(), attribute_description())
            .with_culling(VK_CULL_MODE_BACK_BIT,
                VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .build();

    {
        auto staging_buffer{vkrndr::create_staging_buffer(backend_->device(),
            sizeof(line_vertex_t) * 8)};
        auto map{vkrndr::map_memory(backend_->device(), staging_buffer)};

        auto* const vertices{map.as<line_vertex_t>()};
        vertices[0] = {glm::vec3{-1, -1, -1}, ' ', glm::vec4{0.5f}};
        vertices[1] = {glm::vec3{1, -1, -1}, ' ', glm::vec4{0.5f}};
        vertices[2] = {glm::vec3{1, 1, -1}, ' ', glm::vec4{0.5f}};
        vertices[3] = {glm::vec3{-1, 1, -1}, ' ', glm::vec4{0.5f}};
        vertices[4] = {glm::vec3{-1, -1, 1}, ' ', glm::vec4{0.5f}};
        vertices[5] = {glm::vec3{1, -1, 1}, ' ', glm::vec4{0.5f}};
        vertices[6] = {glm::vec3{1, 1, 1}, ' ', glm::vec4{0.5f}};
        vertices[7] = {glm::vec3{-1, 1, 1}, ' ', glm::vec4{0.5f}};

        unmap_memory(backend_->device(), &map);

        vertex_buffer_ = vkrndr::create_buffer(backend_->device(),
            staging_buffer.size,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        backend_->transfer_buffer(staging_buffer, vertex_buffer_);

        destroy(&backend_->device(), &staging_buffer);
    }

    {
        auto staging_buffer{vkrndr::create_staging_buffer(backend_->device(),
            sizeof(uint32_t) * 36)};
        auto map{vkrndr::map_memory(backend_->device(), staging_buffer)};

        uint32_t* const indices{map.as<uint32_t>()};

        indices[0] = 0;
        indices[1] = 1;
        indices[2] = 2;
        indices[3] = 0;
        indices[4] = 2;
        indices[5] = 3;

        indices[6] = 4;
        indices[7] = 6;
        indices[8] = 5;
        indices[9] = 4;
        indices[10] = 7;
        indices[11] = 6;

        indices[12] = 0;
        indices[13] = 3;
        indices[14] = 7;
        indices[15] = 0;
        indices[16] = 7;
        indices[17] = 4;

        indices[18] = 1;
        indices[19] = 5;
        indices[20] = 6;
        indices[21] = 1;
        indices[22] = 6;
        indices[23] = 2;

        indices[24] = 3;
        indices[25] = 2;
        indices[26] = 6;
        indices[27] = 3;
        indices[28] = 6;
        indices[29] = 7;

        indices[30] = 0;
        indices[31] = 4;
        indices[32] = 5;
        indices[33] = 0;
        indices[34] = 5;
        indices[35] = 1;

        unmap_memory(backend_->device(), &map);

        index_buffer_ = vkrndr::create_buffer(backend_->device(),
            staging_buffer.size,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        backend_->transfer_buffer(staging_buffer, index_buffer_);

        destroy(&backend_->device(), &staging_buffer);
    }
}

galileo::gbuffer_shader_t::~gbuffer_shader_t()
{
    destroy(&backend_->device(), &pipeline_);

    destroy(&backend_->device(), &index_buffer_);

    destroy(&backend_->device(), &vertex_buffer_);
}

VkPipelineLayout galileo::gbuffer_shader_t::pipeline_layout() const
{
    return *pipeline_.layout;
}

void galileo::gbuffer_shader_t::draw(VkCommandBuffer command_buffer,
    gbuffer_t& gbuffer,
    vkrndr::image_t const& depth_buffer)
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

    VkDeviceSize const zero_offset{};
    vkCmdBindVertexBuffers(command_buffer,
        0,
        1,
        &vertex_buffer_.buffer,
        &zero_offset);

    vkCmdBindIndexBuffer(command_buffer,
        index_buffer_.buffer,
        0,
        VK_INDEX_TYPE_UINT32);

    {
        [[maybe_unused]] auto const guard{
            color_pass.begin(command_buffer, {{0, 0}, gbuffer.extent()})};

        vkrndr::bind_pipeline(command_buffer, pipeline_);

        vkCmdDrawIndexed(command_buffer, 36, 2, 0, 0, 0);
    }
}
