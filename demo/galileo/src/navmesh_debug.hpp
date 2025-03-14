#ifndef GALILEO_NAVMESH_DEBUG_INCLUDED
#define GALILEO_NAVMESH_DEBUG_INCLUDED

#include <recastnavigation/DetourNavMesh.h>

class dtNavMeshQuery;
struct rcPolyMesh;
struct rcPolyMeshDetail;

namespace galileo
{
    class batch_renderer_t;
} // namespace galileo

namespace galileo
{
    class [[nodiscard]] navmesh_debug_t final
    {
    public:
        explicit navmesh_debug_t(batch_renderer_t& batch_renderer);

        navmesh_debug_t(navmesh_debug_t const&) = delete;

        navmesh_debug_t(navmesh_debug_t&&) noexcept = delete;

    public:
        ~navmesh_debug_t() = default;

    public:
        void draw_poly_mesh(rcPolyMesh const& poly_mesh);

        void draw_detail_poly_mesh(rcPolyMeshDetail const& poly_mesh_detail);

        void draw_navigation_mesh(dtNavMesh const& navigation_mesh);

        void draw_nodes(dtNavMeshQuery const& query);

        void draw_poly(dtNavMesh const& navigation_mesh, dtPolyRef reference);

    public:
        navmesh_debug_t& operator=(navmesh_debug_t const&) = delete;

        navmesh_debug_t& operator=(navmesh_debug_t&&) noexcept = delete;

    private:
        batch_renderer_t* batch_renderer_;
    };
} // namespace galileo

#endif
