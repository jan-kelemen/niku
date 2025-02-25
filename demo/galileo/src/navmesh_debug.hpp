#ifndef GALILEO_NAVMESH_DEBUG_INCLUDED
#define GALILEO_NAVMESH_DEBUG_INCLUDED

#include <glm/vec3.hpp>

#include <volk.h>

#include <cstdint>
#include <span>

struct rcPolyMesh;
struct rcPolyMeshDetail;

namespace vkrndr
{
    class backend_t;
} // namespace vkrndr

namespace galileo
{
    class batch_renderer_t;
} // namespace galileo

namespace galileo
{
    class [[nodiscard]] navmesh_debug_t final
    {
    public:
        navmesh_debug_t(batch_renderer_t& batch_renderer);

        navmesh_debug_t(navmesh_debug_t const&) = delete;

        navmesh_debug_t(navmesh_debug_t&&) noexcept = delete;

    public:
        ~navmesh_debug_t() = default;

    public:
        void update(rcPolyMesh const& poly_mesh);

        void update(rcPolyMeshDetail const& poly_mesh_detail);

        void update(std::span<glm::vec3 const> const& path_points);

    public:
        navmesh_debug_t& operator=(navmesh_debug_t const&) = delete;

        navmesh_debug_t& operator=(navmesh_debug_t&&) noexcept = delete;

    private:
        batch_renderer_t* batch_renderer_;
    };
} // namespace galileo

#endif
