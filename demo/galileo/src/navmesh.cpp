#include <navmesh.hpp>

#include <cppext_memory.hpp>
#include <cppext_numeric.hpp>

#include <ngnast_mesh_transform.hpp>
#include <ngnast_scene_model.hpp>

#include <glm/gtc/type_ptr.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <stdexcept>
#include <utility>
#include <vector>

galileo::poly_mesh_t galileo::generate_navigation_mesh(
    navmesh_parameters_t const& parameters,
    ngnast::primitive_t const& primitive,
    ngnast::bounding_box_t const& bounding_box)
{
    rcConfig config{
        .cs = parameters.cell_size,
        .ch = parameters.cell_height,
        .walkableSlopeAngle = glm::degrees(parameters.walkable_slope_angle),
        .walkableHeight = static_cast<int>(
            ceilf(parameters.walkable_height / parameters.cell_height)),
        .walkableClimb = static_cast<int>(
            floorf(parameters.walkable_climb / parameters.cell_height)),
        .walkableRadius = static_cast<int>(
            ceilf(parameters.walkable_radius / parameters.cell_size)),
        .maxEdgeLen =
            static_cast<int>(parameters.max_edge_length / parameters.cell_size),
        .maxSimplificationError = parameters.max_simplification_error,
        .minRegionArea = static_cast<int>(sqrtf(parameters.min_region_size)),
        .mergeRegionArea =
            static_cast<int>(sqrtf(parameters.merge_region_size)),
        .maxVertsPerPoly = parameters.max_verts_per_poly,
        .detailSampleDist = (parameters.detail_sample_distance < 0.9f
                                    ? 0.0f
                                    : parameters.detail_sample_distance) *
            parameters.cell_size,
        .detailSampleMaxError =
            parameters.detail_sample_max_error * parameters.cell_height,
    };

    std::ranges::copy_n(glm::value_ptr(bounding_box.min),
        3,
        std::begin(config.bmin));
    std::ranges::copy_n(glm::value_ptr(bounding_box.max),
        3,
        std::begin(config.bmax));

    rcCalcGridSize(config.bmin,
        config.bmax,
        config.cs,
        &config.width,
        &config.height);

    using heightfield_ptr_t =
        cppext::unique_ptr_with_static_deleter_t<rcHeightfield,
            &rcFreeHeightField>;
    heightfield_ptr_t heightfield{rcAllocHeightfield()};
    if (!heightfield)
    {
        throw std::runtime_error{"Can't allocate heightfield"};
    }

    rcContext context;
    if (!rcCreateHeightfield(&context,
            *heightfield,
            config.width,
            config.height,
            config.bmin,
            config.bmax,
            config.cs,
            config.ch))
    {
        throw std::runtime_error{"Can't create heightfield"};
    }

    bool const is_indexed{!primitive.indices.empty()};

    auto const triangle_areas_count{
        (is_indexed ? primitive.indices.size() : primitive.vertices.size()) /
        3};

    std::vector<unsigned char> triangle_areas;
    triangle_areas.insert(triangle_areas.end(), triangle_areas_count, '\0');

    std::vector<float> vertices;
    std::vector<int> indices;
    auto convert_indexed_primitive = [&vertices, &indices](
                                         ngnast::primitive_t const& p) mutable
    {
        vertices.reserve(p.vertices.size() * 3);
        for (auto const& vertex : p.vertices)
        {
            vertices.insert(vertices.end(),
                glm::value_ptr(vertex.position),
                glm::value_ptr(vertex.position) + 3);
        }

        indices.reserve(p.indices.size());
        std::ranges::transform(p.indices,
            std::back_inserter(indices),
            &cppext::narrow<int, unsigned int>);
    };

    if (is_indexed)
    {
        convert_indexed_primitive(primitive);
    }
    else
    {
        auto unindexed_primitive{primitive};
        ngnast::mesh::make_indexed(unindexed_primitive);
        convert_indexed_primitive(primitive);
    }

    rcMarkWalkableTriangles(&context,
        config.walkableSlopeAngle,
        vertices.data(),
        cppext::narrow<int>(vertices.size() / 3),
        indices.data(),
        cppext::narrow<int>(indices.size() / 3),
        triangle_areas.data());
    if (!rcRasterizeTriangles(&context,
            vertices.data(),
            cppext::narrow<int>(vertices.size() / 3),
            indices.data(),
            triangle_areas.data(),
            cppext::narrow<int>(indices.size() / 3),
            *heightfield))
    {
        throw std::runtime_error{"Can't rasterize triangles"};
    }

    triangle_areas.clear();

    rcFilterLowHangingWalkableObstacles(&context,
        config.walkableClimb,
        *heightfield);

    rcFilterLedgeSpans(&context,
        config.walkableHeight,
        config.walkableClimb,
        *heightfield);

    rcFilterWalkableLowHeightSpans(&context,
        config.walkableHeight,
        *heightfield);

    using compact_heightfield_ptr_t =
        cppext::unique_ptr_with_static_deleter_t<rcCompactHeightfield,
            &rcFreeCompactHeightfield>;
    compact_heightfield_ptr_t compact_heightfield{rcAllocCompactHeightfield()};
    if (!heightfield)
    {
        throw std::runtime_error{"Can't allocate compact heightfield"};
    }

    if (!rcBuildCompactHeightfield(&context,
            config.walkableHeight,
            config.walkableClimb,
            *heightfield,
            *compact_heightfield))
    {
        throw std::runtime_error{"Can't build compact data"};
    }

    heightfield.reset();

    // Watershed partitioning
    if (!rcBuildDistanceField(&context, *compact_heightfield))
    {
        throw std::runtime_error{"Can't build distance field"};
    }

    if (!rcBuildRegions(&context,
            *compact_heightfield,
            config.borderSize,
            config.minRegionArea,
            config.mergeRegionArea))
    {
        throw std::runtime_error{"Can't build watershed regions"};
    }

    using contour_set_ptr_t =
        cppext::unique_ptr_with_static_deleter_t<rcContourSet,
            &rcFreeContourSet>;

    contour_set_ptr_t contour_set{rcAllocContourSet()};
    if (!contour_set)
    {
        throw std::runtime_error{"Can't allocate contour set"};
    }

    if (!rcBuildContours(&context,
            *compact_heightfield,
            config.maxSimplificationError,
            config.maxEdgeLen,
            *contour_set))
    {
        throw std::runtime_error{"Can't create contours"};
    }

    poly_mesh_ptr_t poly_mesh{rcAllocPolyMesh()};

    if (!poly_mesh)
    {
        throw std::runtime_error{"Can't allocate poly mesh"};
    }

    if (!rcBuildPolyMesh(&context,
            *contour_set,
            config.maxVertsPerPoly,
            *poly_mesh))
    {
        throw std::runtime_error{"Can't triangulate contours"};
    }

    poly_mesh_detail_ptr_t poly_mesh_detail{rcAllocPolyMeshDetail()};
    if (!poly_mesh_detail)
    {
        throw std::runtime_error{"Can't allocate poly mesh detail"};
    }

    if (!rcBuildPolyMeshDetail(&context,
            *poly_mesh,
            *compact_heightfield,
            config.detailSampleDist,
            config.detailSampleMaxError,
            *poly_mesh_detail))
    {
        throw std::runtime_error{"Can't build detail mesh"};
    }

    compact_heightfield.reset();
    contour_set.reset();

    return {std::move(poly_mesh), std::move(poly_mesh_detail)};
}
