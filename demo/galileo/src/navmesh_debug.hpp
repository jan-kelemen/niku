#ifndef GALILEO_NAVMESH_DEBUG_INCLUDED
#define GALILEO_NAVMESH_DEBUG_INCLUDED

#include <vkrndr_buffer.hpp>
#include <vkrndr_pipeline.hpp>

#include <glm/vec3.hpp>

#include <volk.h>

#include <cstdint>

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
    namespace detail
    {
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
    } // namespace detail

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
        [[nodiscard]] VkPipelineLayout pipeline_layout() const;

        void update(poly_mesh_t const& poly_mesh);

        void update(std::span<glm::vec3 const> const& path_points);

        void draw(VkCommandBuffer command_buffer,
            bool draw_main_mesh,
            bool draw_detail_mesh) const;

    public:
        navmesh_debug_t& operator=(navmesh_debug_t const&) = delete;

        navmesh_debug_t& operator=(navmesh_debug_t&&) noexcept = delete;

    private:
        void draw(VkCommandBuffer command_buffer,
            detail::mesh_draw_data_t const& data) const;

    private:
        vkrndr::backend_t* backend_;

        detail::mesh_draw_data_t main_draw_data_;
        detail::mesh_draw_data_t detail_draw_data_;
        detail::mesh_draw_data_t path_draw_data_;

        vkrndr::pipeline_t triangle_pipeline_;
        vkrndr::pipeline_t line_pipeline_;
        vkrndr::pipeline_t point_pipeline_;
    };
} // namespace galileo

#endif
