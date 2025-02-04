#ifndef GALILEO_NAVMESH_DEBUG_INCLUDED
#define GALILEO_NAVMESH_DEBUG_INCLUDED

#include <vkrndr_buffer.hpp>
#include <vkrndr_pipeline.hpp>

#include <volk.h>

namespace vkrndr
{
    class backend_t;
} // namespace vkrndr

struct rcPolyMesh;

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

        void update(rcPolyMesh const& poly_mesh);

        void draw(VkCommandBuffer command_buffer);

    public:
        navmesh_debug_t& operator=(navmesh_debug_t const&) = delete;

        navmesh_debug_t& operator=(navmesh_debug_t&&) noexcept = delete;

    private:
        vkrndr::backend_t* backend_;

        vkrndr::buffer_t vertex_buffer_;
        uint32_t vertex_count_{};

        vkrndr::pipeline_t pipeline_;
    };
} // namespace galileo

#endif
