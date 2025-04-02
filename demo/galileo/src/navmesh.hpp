#ifndef GALILEO_NAVMESH_INCLUDED
#define GALILEO_NAVMESH_INCLUDED

#include <cppext_memory.hpp>

#include <glm/gtc/constants.hpp>
#include <glm/vec3.hpp>

#include <recastnavigation/DetourNavMesh.h>
#include <recastnavigation/DetourNavMeshQuery.h>
#include <recastnavigation/DetourPathCorridor.h>
#include <recastnavigation/Recast.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

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

    struct [[nodiscard]] polymesh_parameters_t final
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

    poly_mesh_t generate_poly_mesh(polymesh_parameters_t const& parameters,
        ngnast::primitive_t const& primitive,
        ngnast::bounding_box_t const& bounding_box);

    using navigation_mesh_ptr_t =
        cppext::unique_ptr_with_static_deleter_t<dtNavMesh, &dtFreeNavMesh>;

    [[nodiscard]] navigation_mesh_ptr_t generate_navigation_mesh(
        polymesh_parameters_t const& parameters,
        poly_mesh_t const& poly_mesh,
        ngnast::bounding_box_t const& bounding_box);

    using navigation_mesh_query_ptr_t =
        cppext::unique_ptr_with_static_deleter_t<dtNavMeshQuery,
            &dtFreeNavMeshQuery>;

    [[nodiscard]] navigation_mesh_query_ptr_t
    create_query(dtNavMesh const* navigation_mesh, int max_nodes = 2048);

    using path_corridor_ptr_t = std::unique_ptr<dtPathCorridor>;

    [[nodiscard]] path_corridor_ptr_t create_path_corridor(
        int max_nodes = 2048);

    struct [[nodiscard]] nearest_polygon_t final
    {
        dtPolyRef polygon;
        glm::vec3 point;
        bool over_polygon;
    };

    [[nodiscard]] std::optional<nearest_polygon_t> find_nearest_polygon(
        dtNavMeshQuery const& query,
        glm::vec3 point,
        glm::vec3 search_extent);

    struct [[nodiscard]] closest_point_t final
    {
        glm::vec3 point;
        bool over_polygon;
    };

    [[nodiscard]] std::optional<closest_point_t> find_closest_point_on_polygon(
        dtNavMeshQuery const& query,
        dtPolyRef polygon,
        glm::vec3 point);

    struct [[nodiscard]] path_t final
    {
        dtNavMeshQuery* query;
        path_corridor_ptr_t corridor;
    };

    [[nodiscard]] std::optional<path_t> find_path(dtNavMeshQuery& query,
        glm::vec3 starting_position,
        glm::vec3 target_position,
        glm::vec3 search_extent);

    struct [[nodiscard]] path_point_t final
    {
        glm::vec3 vertex;
        dtPolyRef polygon;
        unsigned char flag;
    };

    [[nodiscard]] std::vector<path_point_t>
    find_corners(path_t& path, glm::vec3 current_position, size_t count = 1);
} // namespace galileo

#endif
