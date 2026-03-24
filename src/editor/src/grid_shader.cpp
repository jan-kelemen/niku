#include <grid_shader.hpp>

#include <config.hpp>

#include <cppext_container.hpp>
#include <cppext_pragma_warning.hpp>

#include <vkglsl_shader_set.hpp>

#include <vkrndr_buffer.hpp>
#include <vkrndr_commands.hpp>
#include <vkrndr_debug_utils.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_graphics_pipeline_builder.hpp>
#include <vkrndr_memory.hpp>
#include <vkrndr_pipeline.hpp>
#include <vkrndr_pipeline_layout_builder.hpp>
#include <vkrndr_shader_module.hpp>
#include <vkrndr_synchronization.hpp>
#include <vkrndr_utility.hpp>

#include <boost/scope/defer.hpp>
#include <boost/scope/scope_exit.hpp>
#include <boost/scope/scope_fail.hpp>

#include <glm/vec4.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <system_error>
#include <utility>

// IWYU pragma: no_include <boost/scope/exception_checker.hpp>
// IWYU pragma: no_include <filesystem>
// IWYU pragma: no_include <string_view>
// IWYU pragma: no_forward_declare VkDescriptorSet_T

namespace
{
    struct [[nodiscard]] vertex_t final
    {
        glm::vec4 position;
    };

    constexpr VkDeviceSize vertices_size{4 * sizeof(vertex_t)};
    constexpr VkDeviceSize indices_size{6 * sizeof(uint32_t)};

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
            VkVertexInputAttributeDescription{.location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                .offset = offsetof(vertex_t, position)},
        };

        return descriptions;
    }

    [[nodiscard]] std::expected<vkrndr::buffer_t, std::error_code>
    create_quad_buffer(vkrndr::device_t const& device,
        std::function<std::expected<void, std::error_code>(
            std::function<void(VkCommandBuffer)> const&)> const&
            execute_transfer)
    {
        vkrndr::buffer_t const staging_buffer{
            create_staging_buffer(device, vertices_size + indices_size)};
        {
            vkrndr::mapped_memory_t map{map_memory(device, staging_buffer)};
            vertex_t* const vertices{map.as<vertex_t>()};
            vertices[0] = {.position = {-0.5f, 0.0f, 0.5f, 1.0f}};
            vertices[1] = {.position = {0.5f, 0.0f, 0.5f, 1.0f}};
            vertices[2] = {.position = {-0.5f, 0.0f, -0.5f, 1.0f}};
            vertices[3] = {.position = {0.5f, 0.0f, -0.5f, 1.0f}};

            uint32_t* const indices{map.as<uint32_t>(vertices_size)};
            indices[0] = 0;
            indices[1] = 1;
            indices[2] = 2;
            indices[3] = 2;
            indices[4] = 1;
            indices[5] = 3;

            unmap_memory(device, &map);
        }
        boost::scope::defer_guard destroy_staging_buffer{
            [&device, &buffer = staging_buffer]() { destroy(device, buffer); }};

        vkrndr::buffer_t quad_buffer{create_buffer(device,
            {.size = staging_buffer.size,
                .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                    VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .required_memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT})};
        boost::scope::scope_fail destroy_quad{
            [&device, &buffer = quad_buffer]() { destroy(device, buffer); }};
        VKRNDR_IF_DEBUG_UTILS(
            object_name(device, quad_buffer, "Grid Vertex Buffer"));

        if (std::expected<void, std::error_code> const execute_transfer_result{
                execute_transfer(
                    [&staging_buffer, &quad_buffer](VkCommandBuffer const cb)
                    {
                        vkrndr::copy_buffer_to_buffer(cb,
                            staging_buffer,
                            staging_buffer.size,
                            quad_buffer);

                        VkBufferMemoryBarrier2 const barrier{
                            vkrndr::on_stage(buffer_barrier(quad_buffer),
                                VK_PIPELINE_STAGE_2_TRANSFER_BIT)};

                        vkrndr::wait_for(cb, {}, cppext::as_span(barrier), {});
                    })};
            !execute_transfer)
        {
            return std::unexpected{execute_transfer_result.error()};
        }

        return quad_buffer;
    }
} // namespace

std::expected<editor::grid_shader_t, std::error_code>
editor::create_grid_shader(vkrndr::device_t const& device,
    VkFormat const color_attachment_format,
    std::function<std::expected<void, std::error_code>(
        std::function<void(VkCommandBuffer)> const&)> const& execute_transfer)
{
    vkglsl::shader_set_t shaders{enable_shader_debug_symbols,
        enable_shader_optimization};

    std::expected<vkrndr::shader_module_t, std::error_code> const
        add_vertex_result{add_shader_module_from_path(shaders,
            device,
            VK_SHADER_STAGE_VERTEX_BIT,
            "grid.vert")};
    if (!add_vertex_result)
    {
        return std::unexpected{add_vertex_result.error()};
    }
    boost::scope::defer_guard destroy_vtx{
        [&device, &shd = add_vertex_result.value()]()
        { destroy(device, shd); }};
    VKRNDR_IF_DEBUG_UTILS(
        object_name(device, *add_vertex_result, "Grid Vertex Shader"));

    std::expected<vkrndr::shader_module_t, std::error_code> const
        add_fragment_result{add_shader_module_from_path(shaders,
            device,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            "grid.frag")};
    if (!add_fragment_result)
    {
        return std::unexpected{add_fragment_result.error()};
    }
    boost::scope::defer_guard destroy_frag{
        [&device, &shd = add_fragment_result.value()]()
        { destroy(device, shd); }};
    VKRNDR_IF_DEBUG_UTILS(
        object_name(device, *add_fragment_result, "Grid Fragment Shader"));

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
        "Grid Descriptor Layout"));

    vkrndr::pipeline_layout_t pipeline_layout{
        vkrndr::pipeline_layout_builder_t{device}
            .add_descriptor_set_layout(*descriptor_layout_result)
            .build()};
    boost::scope::scope_exit destroy_pipeline_layout{
        [&device, &layout = pipeline_layout]() { destroy(device, layout); }};
    VKRNDR_IF_DEBUG_UTILS(
        object_name(device, pipeline_layout, "Grid Pipeline Layout"));

    VkPipelineColorBlendAttachmentState const alpha_blend{
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
            VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT,
    };

    vkrndr::pipeline_t pipeline{
        vkrndr::graphics_pipeline_builder_t{device, pipeline_layout}
            .add_vertex_input(binding_description(), attribute_description())
            .add_shader(as_pipeline_shader(*add_vertex_result))
            .add_shader(as_pipeline_shader(*add_fragment_result))
            .add_color_attachment(color_attachment_format, alpha_blend)
            .build()};
    boost::scope::scope_exit destroy_pipeline{
        [&device, &pipeline = pipeline]() { destroy(device, pipeline); }};
    VKRNDR_IF_DEBUG_UTILS(object_name(device, pipeline, "Grid Pipeline"));

    if (std::expected<vkrndr::buffer_t, std::error_code> quad_buffer_result{
            create_quad_buffer(device, execute_transfer)})
    {
        std::expected<grid_shader_t, std::error_code> rv{
            std::in_place,
            std::move(quad_buffer_result).value(),
            std::move(descriptor_layout_result).value(),
            pipeline_layout,
            pipeline,
        };

        destroy_descriptor_layout.set_active(false);
        destroy_pipeline_layout.set_active(false);
        destroy_pipeline.set_active(false);

        DISABLE_WARNING_PUSH
        DISABLE_WARNING_PESSIMIZING_MOVE
        // NOLINTNEXTLINE(clang-diagnostic-pessimizing-move)
        return std::move(rv);
        DISABLE_WARNING_POP
    }
    else
    {
        return std::unexpected{quad_buffer_result.error()};
    }
}

void editor::destroy(vkrndr::device_t const& device,
    grid_shader_t const& shader)
{
    destroy(device, shader.pipeline);
    destroy(device, shader.pipeline_layout);
    vkDestroyDescriptorSetLayout(device, shader.descriptor_layout, nullptr);
    destroy(device, shader.quad_buffer);
}

void editor::draw(grid_shader_t const& shader,
    VkCommandBuffer const command_buffer)
{
    vkrndr::bind_pipeline(command_buffer, shader.pipeline);

    static constexpr VkDeviceSize zero_offset{};
    vkCmdBindVertexBuffers(command_buffer,
        0,
        1,
        &shader.quad_buffer.handle,
        &zero_offset);

    vkCmdBindIndexBuffer(command_buffer,
        shader.quad_buffer.handle,
        vertices_size,
        VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(command_buffer, 6, 1, 0, 0, 0);
}
