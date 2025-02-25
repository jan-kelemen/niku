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

galileo::navmesh_debug_t::navmesh_debug_t(batch_renderer_t& batch_renderer)
    : batch_renderer_{&batch_renderer}
{
}

void galileo::navmesh_debug_t::update(rcPolyMesh const& poly_mesh)
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
        return {.position = make_position(v[0], v[1], v[2]), .color = color};
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

void galileo::navmesh_debug_t::update(rcPolyMeshDetail const& poly_mesh_detail)
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
                .color = point_color});
    }
}

void galileo::navmesh_debug_t::update(
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

        batch_renderer_->add_point(
            {.position = point, .color = glm::vec4{1.0f, 1.0f, 0.0f, 0.8f}});
    }
}
