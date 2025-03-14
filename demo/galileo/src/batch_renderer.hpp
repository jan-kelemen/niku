#ifndef GALILEO_BATCH_RENDERER_INCLUDED
#define GALILEO_BATCH_RENDERER_INCLUDED

#include <cppext_cycled_buffer.hpp>

#include <vkrndr_buffer.hpp>
#include <vkrndr_memory.hpp>
#include <vkrndr_pipeline.hpp>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <volk.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace vkrndr
{
    class backend_t;
} // namespace vkrndr

namespace galileo
{
    struct [[nodiscard]] batch_vertex_t final
    {
        glm::vec3 position;
        float width;
        glm::vec4 color;
    };

    class [[nodiscard]] batch_renderer_t final
    {
    public:
        batch_renderer_t(vkrndr::backend_t& backend,
            VkDescriptorSetLayout frame_info_layout,
            VkFormat depth_buffer_format);

        batch_renderer_t(batch_renderer_t const&) = delete;

        batch_renderer_t(batch_renderer_t&&) noexcept = delete;

    public:
        ~batch_renderer_t();

    public:
        void add_triangle(batch_vertex_t const& p1,
            batch_vertex_t const& p2,
            batch_vertex_t const& p3);

        void add_line(batch_vertex_t const& p1, batch_vertex_t const& p2);

        void add_point(batch_vertex_t const& p1);

    public:
        void begin_frame();

        [[nodiscard]] VkPipelineLayout pipeline_layout() const;

        void draw(VkCommandBuffer command_buffer);

    public:
        batch_renderer_t& operator=(batch_renderer_t const&) = delete;

        batch_renderer_t& operator=(batch_renderer_t&&) noexcept = delete;

    private:
        struct [[nodiscard]] vertex_buffer_t final
        {
            VkPrimitiveTopology topology;
            vkrndr::buffer_t buffer;
            vkrndr::mapped_memory_t memory;
            uint32_t size;
            uint32_t capacity;
        };

        struct [[nodiscard]] frame_data_t final
        {
            std::vector<vertex_buffer_t> buffers;
        };

    private:
        vertex_buffer_t* buffer_for(VkPrimitiveTopology topology);

    private:
        vkrndr::backend_t* backend_;

        vkrndr::pipeline_t triangle_pipeline_;
        vkrndr::pipeline_t line_pipeline_;
        vkrndr::pipeline_t point_pipeline_;

        cppext::cycled_buffer_t<frame_data_t> frame_data_;

        std::optional<size_t> triangle_buffer_;
        std::optional<size_t> line_buffer_;
        std::optional<size_t> point_buffer_;
    };
} // namespace galileo

#endif
