#ifndef GALILEO_NAVMESH_INCLUDED
#define GALILEO_NAVMESH_INCLUDED

#include <cppext_memory.hpp>

#include <glm/gtc/constants.hpp>

#include <recastnavigation/DetourNavMesh.h>
#include <recastnavigation/Recast.h>

namespace ngnast
{
    struct primitive_t;
    struct bounding_box_t;
} // namespace ngnast

namespace galileo
{
    enum class partition_type_t
    {
        watershed,
        monotone,
        layers
    };

    struct [[nodiscard]] navmesh_parameters_t final
    {
        float cell_size{0.3f};
        float cell_height{0.2f};
        float walkable_slope_angle{glm::quarter_pi<float>()};
        float walkable_height{2.0f};
        float walkable_climb{0.9f};
        float walkable_radius{0.6f};
        float max_edge_length{12.0f};
        float max_simplification_error{1.3f};
        float min_region_size{8};
        float merge_region_size{20};
        int max_verts_per_poly{DT_VERTS_PER_POLYGON};
        float detail_sample_distance{6.0f};
        float detail_sample_max_error{1.0f};
        partition_type_t partition_type{partition_type_t::watershed};
    };

    using poly_mesh_ptr_t =
        cppext::unique_ptr_with_static_deleter_t<rcPolyMesh, &rcFreePolyMesh>;
    using poly_mesh_detail_ptr_t =
        cppext::unique_ptr_with_static_deleter_t<rcPolyMeshDetail,
            &rcFreePolyMeshDetail>;

    struct [[nodiscard]] poly_mesh_t final
    {
        poly_mesh_ptr_t mesh;
        poly_mesh_detail_ptr_t detail_mesh;
    };

    poly_mesh_t generate_navigation_mesh(navmesh_parameters_t const& parameters,
        ngnast::primitive_t const& primitive,
        ngnast::bounding_box_t const& bounding_box);
} // namespace galileo

#endif
