#ifndef GALILEO_NAVMESH_DEBUG_INCLUDED
#define GALILEO_NAVMESH_DEBUG_INCLUDED

#include <navmesh.hpp>

#include <vkrndr_buffer.hpp>
#include <vkrndr_pipeline.hpp>

#include <volk.h>

namespace vkrndr
{
    class backend_t;
} // namespace vkrndr

namespace galileo
{
    struct poly_mesh_t;
} // namespace galileo

namespace galileo
{
    class [[nodiscard]] navmesh_debug_t final
    {
    public:
        navmesh_debug_t(vkrndr::backend_t& backend,
            VkDescriptorSetLayout frame_info_layout,
            VkFormat depth_buffer_format);

        navmesh_debug_t(navmesh_debug_t const&) = delete;

        navmesh_debug_t(navmesh_debug_t&&) noexcept = delete;

    public:
        ~navmesh_debug_t();

    public:
        [[nodiscard]] VkPipelineLayout pipeline_layout();

        void update(poly_mesh_t const& poly_mesh);

        void draw(VkCommandBuffer command_buffer);

    public:
        navmesh_debug_t& operator=(navmesh_debug_t const&) = delete;

        navmesh_debug_t& operator=(navmesh_debug_t&&) noexcept = delete;

    private:
        struct [[nodiscard]] mesh_draw_data_t final
        {
            vkrndr::buffer_t vertex_buffer;

            uint32_t max_triangles{};
            uint32_t triangles{};
            uint32_t max_lines{};
            uint32_t lines{};
            uint32_t max_points{};
            uint32_t points{};
        };

    private:
        void draw(VkCommandBuffer command_buffer, mesh_draw_data_t const& data);

    private:
        vkrndr::backend_t* backend_;

        mesh_draw_data_t main_draw_data_;
        mesh_draw_data_t detail_draw_data_;

        vkrndr::pipeline_t triangle_pipeline_;
        vkrndr::pipeline_t line_pipeline_;
        vkrndr::pipeline_t point_pipeline_;
    };
} // namespace galileo

#endif
