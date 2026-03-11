#ifndef EDITOR_GRID_SHADER_INCLUDED
#define EDITOR_GRID_SHADER_INCLUDED

#include <vkrndr_buffer.hpp>
#include <vkrndr_pipeline.hpp>

#include <volk.h>

#include <expected>
#include <functional>
#include <system_error>

namespace vkrndr
{
    struct device_t;
} // namespace vkrndr

namespace editor
{
    struct [[nodiscard]] grid_shader_t final
    {
        vkrndr::buffer_t quad_buffer;
        VkDescriptorSetLayout descriptor_layout;
        vkrndr::pipeline_layout_t pipeline_layout;
        vkrndr::pipeline_t pipeline;
    };

    [[nodiscard]] std::expected<grid_shader_t, std::error_code>
    create_grid_shader(vkrndr::device_t const& device,
        VkFormat color_attachment_format,
        std::function<std::expected<void, std::error_code>(
            std::function<void(VkCommandBuffer)> const&)> const&
            execute_transfer);

    void destroy(vkrndr::device_t const& device, grid_shader_t const& shader);

    void draw(grid_shader_t const& shader, VkCommandBuffer command_buffer);
} // namespace editor

#endif
