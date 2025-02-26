#include <navmesh_debug.hpp>

#include <batch_renderer.hpp>
#include <config.hpp>
#include <navmesh.hpp>

#include <cppext_numeric.hpp>

#include <vkglsl_shader_set.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_buffer.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_memory.hpp>
#include <vkrndr_pipeline.hpp>
#include <vkrndr_shader_module.hpp>

#include <boost/scope/defer.hpp>
#include <boost/scope/scope_fail.hpp>

#include <glm/gtc/type_ptr.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <recastnavigation/Recast.h>

#include <vma_impl.hpp>

#include <volk.h>

#include <array>
#include <cassert>
#include <cstddef>
#include <random>

// IWYU pragma: no_include <boost/scope/exception_checker.hpp>
// IWYU pragma: no_include <expected>
// IWYU pragma: no_include <filesystem>
// IWYU pragma: no_include <initializer_list>
// IWYU pragma: no_include <memory>
// IWYU pragma: no_include <optional>
// IWYU pragma: no_include <span>
// IWYU pragma: no_include <system_error>

namespace
{
    void draw_poly_boundaries(galileo::batch_renderer_t& batch_renderer,
        dtMeshTile const& tile,
        glm::vec4 const base_color,
        bool const inner)
    {
        auto const distance_2d = [](float const* const pt,
                                     float const* const p,
                                     float const* const q)
        {
            float const pqx{q[0] - p[0]};
            float const pqz{q[2] - p[2]};
            float dx{pt[0] - p[0]};
            float dz{pt[2] - p[2]};
            float const d{pqx * pqx + pqz * pqz};
            float t{pqx * dx + pqz * dz};

            if (d != 0)
            {
                t /= d;
            }

            dx = p[0] + t * pqx - pt[0];
            dz = p[2] + t * pqz - pt[2];

            return dx * dx + dz * dz;
        };

        for (int i{}; i != tile.header->polyCount; ++i)
        {
            dtPoly const& poly{tile.polys[i]};
            dtPolyDetail const& poly_detail{tile.detailMeshes[i]};

            for (int j{}; j != poly.vertCount; ++j)
            {
                glm::vec4 color{base_color};
                if (inner)
                {
                    if (poly.neis[j] == 0)
                    {
                        continue;
                    }

                    if (poly.neis[j] & DT_EXT_LINK)
                    {
                        bool connection{false};
                        for (unsigned int k{poly.firstLink}; k != DT_NULL_LINK;
                            k = tile.links[k].next)
                        {
                            if (tile.links[k].edge == j)
                            {
                                connection = true;
                            }
                        }

                        color = connection ? glm::vec4{1.0f, 1.0f, 1.0f, 0.2f}
                                           : glm::vec4{0.0f, 0.0f, 0.0f, 0.2f};
                    }
                    else
                    {
                        color = glm::vec4{0.0f, 0.2f, 0.4f, 0.3f};
                    }
                }
                else
                {
                    if (poly.neis[j] != 0)
                    {
                        continue;
                    }
                }

                float const* const v0{&tile.verts[poly.verts[j] * 3]};
                float const* const v1{
                    &tile.verts[poly.verts[(j + 1) % poly.vertCount] * 3]};

                auto const make_tv = [&tile, &poly, &poly_detail](
                                         unsigned char const tv)
                {
                    return tv < poly.vertCount
                        ? &tile.verts[poly.verts[tv] * 3]
                        : &tile.detailVerts[(poly_detail.vertBase + tv -
                                                poly.vertCount) *
                              3];
                };

                for (int k{}; k != poly_detail.triCount; ++k)
                {
                    unsigned char const* const t{
                        &tile.detailTris[(poly_detail.triBase + k) * 4]};
                    std::array<float*, 3> const tv{make_tv(t[0]),
                        make_tv(t[1]),
                        make_tv(t[2])};

                    for (int m{}, n{2}; m < 3; n = m, ++m)
                    {
                        if ((dtGetDetailTriEdgeFlags(t[3], n) &
                                DT_DETAIL_EDGE_BOUNDARY) == 0)
                        {
                            continue;
                        }

                        constexpr float threshold{0.01f * 0.01f};
                        if (distance_2d(tv[n], v0, v1) < threshold &&
                            distance_2d(tv[m], v0, v1) < threshold)
                        {
                            batch_renderer.add_line(
                                {.position = glm::make_vec3(tv[n]),
                                    .color = color},
                                {.position = glm::make_vec3(tv[m]),
                                    .color = color});
                        }
                    }
                }
            }
        }
    }

    void draw_mesh_tile(galileo::batch_renderer_t& batch_renderer,
        dtNavMesh const& navigation_mesh,
        dtMeshTile const& tile)
    {
        dtPolyRef const base{navigation_mesh.getPolyRefBase(&tile)};

        unsigned int const tile_number{navigation_mesh.decodePolyIdTile(base)};
        glm::vec4 const tile_color{0.5f, 0.5f, 0.5f, 0.5f};

        for (int i{}; i != tile.header->polyCount; ++i)
        {
            dtPoly const& poly{tile.polys[i]};
            dtPolyDetail const& poly_detail{tile.detailMeshes[i]};

            auto const make_vertex =
                [&tile, &poly, &poly_detail](
                    unsigned char const tv) -> galileo::batch_vertex_t
            {
                auto const color{cppext::as_fp(poly.getArea()) / 255.0f};

                float const* const position_data{tv < poly.vertCount
                        ? &tile.verts[poly.verts[tv] * 3]
                        : &tile.detailVerts[(poly_detail.vertBase + tv -
                                                poly.vertCount) *
                              3]};

                return {.position = glm::make_vec3(position_data) +
                        glm::vec3{0.01f, 0.01f, 0.01f},
                    .color = glm::vec4{color, color, color, 0.5f}};
            };

            for (int j{}; j != poly_detail.triCount; ++j)
            {
                unsigned char const* const t{
                    &tile.detailTris[(poly_detail.triBase + j) * 4]};

                batch_renderer.add_triangle(make_vertex(t[0]),
                    make_vertex(t[1]),
                    make_vertex(t[2]));
            }
        }

        draw_poly_boundaries(batch_renderer,
            tile,
            {0.0f, 0.2f, 0.3f, 0.2f},
            true);

        draw_poly_boundaries(batch_renderer,
            tile,
            {0.2f, 0.2f, 0.3f, 0.8f},
            false);

        for (int i{}; i != tile.header->vertCount; ++i)
        {
            batch_renderer.add_point(
                {.position = glm::make_vec3(&tile.verts[i * 3]),
                    .width = 10.0f,
                    .color = glm::vec4{0.0f, 0.0f, 0.0f, 0.75f}});
        }
    }
} // namespace

galileo::navmesh_debug_t::navmesh_debug_t(batch_renderer_t& batch_renderer)
    : batch_renderer_{&batch_renderer}
{
}

void galileo::navmesh_debug_t::draw_poly_mesh(rcPolyMesh const& poly_mesh)
{
    int const vertices_per_polygon{poly_mesh.nvp};

    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    auto const make_position = [orig = glm::make_vec3(poly_mesh.bmin),
                                   cell_size = poly_mesh.cs,
                                   cell_height = poly_mesh.ch](float const x,
                                   float const y,
                                   float const z)
    {
        return orig +
            glm::vec3{x * cell_size, y * cell_height + 0.01f, z * cell_size};
    };
    // NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay)

    auto const make_vertex = [&poly_mesh, &make_position](auto const i,
                                 glm::vec4 const color) -> batch_vertex_t
    {
        unsigned short const* const v{
            &poly_mesh.verts[cppext::narrow<ptrdiff_t>(i * 3)]};
        return {.position = make_position(v[0], v[1], v[2]),
            .width = 10.0f,
            .color = color};
    };

    for (int i{}; i != poly_mesh.npolys; ++i)
    {
        unsigned short const* const polygon{
            &poly_mesh.polys[cppext::narrow<ptrdiff_t>(
                i * vertices_per_polygon * 2)]};

        glm::vec4 const color{[](unsigned char const area) -> glm::vec4
                {
                    switch (area)
                    {
                    case RC_WALKABLE_AREA:
                        return {0.0f, 192.0f, 255.0f, 64.0f};
                    case RC_NULL_AREA:
                        return {0.0f, 0.0f, 0.0f, 64.0f};
                    default:
                    {
                        float const v{cppext::as_fp(area)};
                        return {v, v, v, 64.0f};
                    }
                    }
                }(poly_mesh.areas[i]) /
                255.0f};

        for (int j{2}; j != vertices_per_polygon; ++j)
        {
            if (polygon[j] == RC_MESH_NULL_IDX)
            {
                break;
            }

            batch_renderer_->add_triangle(make_vertex(polygon[0], color),
                make_vertex(polygon[j - 1], color),
                make_vertex(polygon[j], color));
        }
    }

    glm::vec4 const line_color{1.0f,
        48.0f / 255.0f,
        64.0f / 255.0f,
        220.0f / 255.0f};
    for (int i{}; i != poly_mesh.npolys; ++i)
    {
        unsigned short const* const polygon{
            &poly_mesh.polys[cppext::narrow<ptrdiff_t>(
                i * vertices_per_polygon * 2)]};
        for (int j{}; j != vertices_per_polygon; ++j)
        {
            if (polygon[j] == RC_MESH_NULL_IDX)
            {
                break;
            }

            if ((polygon[vertices_per_polygon + j] & 0x8000) == 0)
            {
                continue;
            }

            int const nj{(j + 1 >= vertices_per_polygon ||
                             polygon[j + 1] == RC_MESH_NULL_IDX)
                    ? 0
                    : j + 1};

            auto col{line_color};
            if ((polygon[vertices_per_polygon + j] & 0xf) != 0xf)
            {
                col = glm::vec4{1.0f, 1.0f, 1.0f, 0.5f};
            }

            batch_renderer_->add_line(make_vertex(polygon[j], col),
                make_vertex(polygon[nj], col));
        }
    }

    constexpr glm::vec4 point_color{0.0f, 0.0f, 0.0f, 220.0f / 255.0f};
    auto const nverts{cppext::narrow<ptrdiff_t>(poly_mesh.nverts)};
    for (ptrdiff_t i{}; i != nverts; ++i)
    {
        batch_renderer_->add_point(make_vertex(i, point_color));
    }
}

void galileo::navmesh_debug_t::draw_detail_poly_mesh(
    rcPolyMeshDetail const& poly_mesh_detail)
{
    std::minstd_rand generator;

    for (int i{}; i != poly_mesh_detail.nmeshes; ++i)
    {
        unsigned int const* const m{
            &poly_mesh_detail.meshes[cppext::narrow<ptrdiff_t>(i * 4)]};
        unsigned int const bverts{m[0]};
        unsigned int const btris{m[2]};

        float const* const verts{
            &poly_mesh_detail.verts[cppext::narrow<ptrdiff_t>(bverts * 3)]};
        unsigned char const* const tris{
            &poly_mesh_detail.tris[cppext::narrow<ptrdiff_t>(btris * 4)]};

        float const v{std::generate_canonical<float, 10>(generator)};
        glm::vec4 const color{v, v, v, 0.5f};

        auto const ntris{cppext::narrow<ptrdiff_t>(m[3])};
        for (ptrdiff_t j{}; j != ntris; ++j)
        {
            batch_renderer_->add_triangle(
                {.position = glm::make_vec3(
                     &verts[cppext::narrow<ptrdiff_t>(tris[j * 4 + 0] * 3)]),
                    .color = color},
                {.position = glm::make_vec3(
                     &verts[cppext::narrow<ptrdiff_t>(tris[j * 4 + 1] * 3)]),
                    .color = color},
                {.position = glm::make_vec3(
                     &verts[cppext::narrow<ptrdiff_t>(tris[j * 4 + 2] * 3)]),
                    .color = color});
        }
    }

    for (int i{}; i != poly_mesh_detail.nmeshes; ++i)
    {
        unsigned int const* const m{
            &poly_mesh_detail.meshes[cppext::narrow<ptrdiff_t>(i * 4)]};
        unsigned int const bverts{m[0]};
        unsigned int const btris{m[2]};

        float const* const verts{
            &poly_mesh_detail.verts[cppext::narrow<ptrdiff_t>(bverts * 3)]};
        unsigned char const* const tris{
            &poly_mesh_detail.tris[cppext::narrow<ptrdiff_t>(btris * 4)]};

        auto const ntris{cppext::narrow<ptrdiff_t>(m[3])};
        for (ptrdiff_t j{}; j != ntris; ++j)
        {
            unsigned char const* const t{&tris[j * 4]};

            for (ptrdiff_t k{}, kp{2}; k != 3; kp = k++)
            {
                auto const ef{
                    cppext::narrow<unsigned char>((t[3] >> (kp * 2)) & 0x3)};
                if (ef == 0)
                {
                    // Internal edges
                    if (t[kp] < t[k])
                    {
                        constexpr glm::vec4 internal_color{0.0f,
                            0.0f,
                            1.0f,
                            0.5f};
                        batch_renderer_->add_line(
                            {.position = glm::make_vec3(
                                 &verts[cppext::narrow<ptrdiff_t>(t[kp] * 3)]),
                                .color = internal_color},
                            {.position = glm::make_vec3(
                                 &verts[cppext::narrow<ptrdiff_t>(t[k] * 3)]),
                                .color = internal_color});
                    }
                }
                else
                {
                    // External edges
                    constexpr glm::vec4 external_color{0.0f, 1.0f, 0.0f, 0.5f};
                    batch_renderer_->add_line(
                        {.position = glm::make_vec3(
                             &verts[cppext::narrow<ptrdiff_t>(t[kp] * 3)]),
                            .color = external_color},
                        {.position = glm::make_vec3(
                             &verts[cppext::narrow<ptrdiff_t>(t[k] * 3)]),
                            .color = external_color});
                }
            }
        }
    }

    constexpr glm::vec4 point_color{0.0f, 0.0f, 0.0f, 220.0f / 255.0f};
    auto const nverts{cppext::narrow<ptrdiff_t>(poly_mesh_detail.nverts)};
    for (ptrdiff_t i{}; i != nverts; ++i)
    {
        batch_renderer_->add_point(
            {.position = glm::make_vec3(&poly_mesh_detail.verts[i * 3]),
                .width = 10.0f,
                .color = point_color});
    }
}

void galileo::navmesh_debug_t::draw_path_points(
    std::span<glm::vec3 const> const& path_points)
{
    for (size_t i{}; i != path_points.size(); ++i)
    {
        auto const& point{path_points[i]};

        if (i != path_points.size() - 1)
        {
            batch_renderer_->add_line(
                {.position = point, .color = glm::vec4{1.0f, 1.0f, 0.0f, 0.6f}},
                {.position = path_points[i + 1],
                    .color = glm::vec4{1.0f, 1.0f, 0.0f, 0.6f}});
        }

        batch_renderer_->add_point({.position = point,
            .width = 10.0f,
            .color = glm::vec4{1.0f, 1.0f, 0.0f, 0.8f}});
    }
}

void galileo::navmesh_debug_t::draw_navigation_mesh(
    dtNavMesh const& navigation_mesh)
{
    for (int i{}; i != navigation_mesh.getMaxTiles(); ++i)
    {
        dtMeshTile const* const tile{navigation_mesh.getTile(i)};
        if (!tile || !tile->header)
        {
            continue;
        }

        draw_mesh_tile(*batch_renderer_, navigation_mesh, *tile);
    }
}
