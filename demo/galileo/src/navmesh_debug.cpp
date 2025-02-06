#include <navmesh_debug.hpp>

#include <config.hpp>
#include <navmesh.hpp>

#include <cppext_numeric.hpp>

#include <vkglsl_shader_set.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_buffer.hpp>
#include <vkrndr_memory.hpp>
#include <vkrndr_pipeline.hpp>
#include <vkrndr_shader_module.hpp>

#include <boost/scope/defer.hpp>

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

// IWYU pragma: no_include <expected>
// IWYU pragma: no_include <filesystem>
// IWYU pragma: no_include <initializer_list>
// IWYU pragma: no_include <memory>
// IWYU pragma: no_include <optional>
// IWYU pragma: no_include <span>
// IWYU pragma: no_include <system_error>

namespace
{
    struct [[nodiscard]] vertex_t final
    {
        glm::vec3 position;
        char padding;
        glm::vec4 color;
    };

    consteval auto binding_description()
    {
        constexpr std::array descriptions{
            VkVertexInputBindingDescription{.binding = 0,
                .stride = sizeof(vertex_t),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX},
        };

        return descriptions;
    }

    consteval auto attribute_description()
    {
        constexpr std::array descriptions{
            VkVertexInputAttributeDescription{.location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(vertex_t, position)},
            VkVertexInputAttributeDescription{.location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                .offset = offsetof(vertex_t, color)},
        };

        return descriptions;
    }

    //{
    //	dd->begin(DU_DRAW_TRIS);
    //
    //	for (int i = 0; i < dmesh.nmeshes; ++i)
    //	{
    //		const unsigned int* m = &dmesh.meshes[i*4];
    //		const unsigned int bverts = m[0];
    //		const unsigned int btris = m[2];
    //		const int ntris = (int)m[3];
    //		const float* verts = &dmesh.verts[bverts*3];
    //		const unsigned char* tris = &dmesh.tris[btris*4];
    //
    //		unsigned int color = duIntToCol(i, 192);
    //
    //		for (int j = 0; j < ntris; ++j)
    //		{
    //			dd->vertex(&verts[tris[j*4+0]*3], color);
    //			dd->vertex(&verts[tris[j*4+1]*3], color);
    //			dd->vertex(&verts[tris[j*4+2]*3], color);
    //		}
    //	}
    //	dd->end();
    //
    //	// Internal edges.
    //	dd->begin(DU_DRAW_LINES, 1.0f);
    //	const unsigned int coli = duRGBA(0,0,0,64);
    //	for (int i = 0; i < dmesh.nmeshes; ++i)
    //	{
    //		const unsigned int* m = &dmesh.meshes[i*4];
    //		const unsigned int bverts = m[0];
    //		const unsigned int btris = m[2];
    //		const int ntris = (int)m[3];
    //		const float* verts = &dmesh.verts[bverts*3];
    //		const unsigned char* tris = &dmesh.tris[btris*4];
    //
    //		for (int j = 0; j < ntris; ++j)
    //		{
    //			const unsigned char* t = &tris[j*4];
    //			for (int k = 0, kp = 2; k < 3; kp=k++)
    //			{
    //				unsigned char ef = (t[3] >> (kp*2)) & 0x3;
    //				if (ef == 0)
    //				{
    //					// Internal edge
    //					if (t[kp] < t[k])
    //					{
    //						dd->vertex(&verts[t[kp]*3], coli);
    //						dd->vertex(&verts[t[k]*3], coli);
    //					}
    //				}
    //			}
    //		}
    //	}
    //	dd->end();
    //
    //	// External edges.
    //	dd->begin(DU_DRAW_LINES, 2.0f);
    //	const unsigned int cole = duRGBA(0,0,0,64);
    //	for (int i = 0; i < dmesh.nmeshes; ++i)
    //	{
    //		const unsigned int* m = &dmesh.meshes[i*4];
    //		const unsigned int bverts = m[0];
    //		const unsigned int btris = m[2];
    //		const int ntris = (int)m[3];
    //		const float* verts = &dmesh.verts[bverts*3];
    //		const unsigned char* tris = &dmesh.tris[btris*4];
    //
    //		for (int j = 0; j < ntris; ++j)
    //		{
    //			const unsigned char* t = &tris[j*4];
    //			for (int k = 0, kp = 2; k < 3; kp=k++)
    //			{
    //				unsigned char ef = (t[3] >> (kp*2)) & 0x3;
    //				if (ef != 0)
    //				{
    //					// Ext edge
    //					dd->vertex(&verts[t[kp]*3], cole);
    //					dd->vertex(&verts[t[k]*3], cole);
    //				}
    //			}
    //		}
    //	}
    //	dd->end();
    //
    //	dd->begin(DU_DRAW_POINTS, 3.0f);
    //	const unsigned int colv = duRGBA(0,0,0,64);
    //	for (int i = 0; i < dmesh.nmeshes; ++i)
    //	{
    //		const unsigned int* m = &dmesh.meshes[i*4];
    //		const unsigned int bverts = m[0];
    //		const int nverts = (int)m[1];
    //		const float* verts = &dmesh.verts[bverts*3];
    //		for (int j = 0; j < nverts; ++j)
    //			dd->vertex(&verts[j*3], colv);
    //	}
    //	dd->end();
    //}
} // namespace

galileo::navmesh_debug_t::navmesh_debug_t(vkrndr::backend_t& backend,
    VkDescriptorSetLayout frame_info_layout,
    VkFormat depth_buffer_format)
    : backend_{&backend}
{
    vkglsl::shader_set_t shader_set{enable_shader_debug_symbols,
        enable_shader_optimization};

    auto vertex_shader{add_shader_module_from_path(shader_set,
        backend_->device(),
        VK_SHADER_STAGE_VERTEX_BIT,
        "navmesh_debug.vert")};
    assert(vertex_shader);
    [[maybe_unused]] boost::scope::defer_guard const destroy_vtx{
        [this, shd = &vertex_shader.value()]()
        { destroy(&backend_->device(), shd); }};

    auto fragment_shader{add_shader_module_from_path(shader_set,
        backend_->device(),
        VK_SHADER_STAGE_FRAGMENT_BIT,
        "navmesh_debug.frag")};
    assert(fragment_shader);
    [[maybe_unused]] boost::scope::defer_guard const destroy_frag{
        [this, shd = &fragment_shader.value()]()
        { destroy(&backend_->device(), shd); }};

    VkPipelineColorBlendAttachmentState const alpha_blend{
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    triangle_pipeline_ =
        vkrndr::pipeline_builder_t{backend_->device(),
            vkrndr::pipeline_layout_builder_t{backend_->device()}
                .add_descriptor_set_layout(frame_info_layout)
                .build()}
            .add_shader(as_pipeline_shader(*vertex_shader))
            .add_shader(as_pipeline_shader(*fragment_shader))
            .add_color_attachment(VK_FORMAT_R16G16B16A16_SFLOAT, alpha_blend)
            .with_depth_test(depth_buffer_format,
                VK_COMPARE_OP_LESS_OR_EQUAL,
                false)
            .add_vertex_input(binding_description(), attribute_description())
            .build();

    line_pipeline_ =
        vkrndr::pipeline_builder_t{backend_->device(),
            triangle_pipeline_.layout}
            .with_primitive_topology(VK_PRIMITIVE_TOPOLOGY_LINE_LIST)
            .with_dynamic_state(VK_DYNAMIC_STATE_LINE_WIDTH)
            .add_shader(as_pipeline_shader(*vertex_shader))
            .add_shader(as_pipeline_shader(*fragment_shader))
            .add_color_attachment(VK_FORMAT_R16G16B16A16_SFLOAT, alpha_blend)
            .with_depth_test(depth_buffer_format,
                VK_COMPARE_OP_LESS_OR_EQUAL,
                false)
            .add_vertex_input(binding_description(), attribute_description())
            .build();

    point_pipeline_ =
        vkrndr::pipeline_builder_t{backend_->device(),
            triangle_pipeline_.layout}
            .with_primitive_topology(VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
            .add_shader(as_pipeline_shader(*vertex_shader))
            .add_shader(as_pipeline_shader(*fragment_shader))
            .add_color_attachment(VK_FORMAT_R16G16B16A16_SFLOAT, alpha_blend)
            .with_depth_test(depth_buffer_format,
                VK_COMPARE_OP_LESS_OR_EQUAL,
                false)
            .add_vertex_input(binding_description(), attribute_description())
            .build();
}

galileo::navmesh_debug_t::~navmesh_debug_t()
{
    destroy(&backend_->device(), &point_pipeline_);
    destroy(&backend_->device(), &line_pipeline_);
    destroy(&backend_->device(), &triangle_pipeline_);

    destroy(&backend_->device(), &detail_draw_data_.vertex_buffer);
    destroy(&backend_->device(), &main_draw_data_.vertex_buffer);
}

VkPipelineLayout galileo::navmesh_debug_t::pipeline_layout() const
{
    return *triangle_pipeline_.layout;
}

void galileo::navmesh_debug_t::update(poly_mesh_t const& poly_mesh)
{
    int const vertices_per_polygon{poly_mesh.mesh->nvp};
    float const cell_size{poly_mesh.mesh->cs};
    float const cell_height{poly_mesh.mesh->ch};

    // Update poly mesh
    {
        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
        auto const make_pos =
            [orig = glm::make_vec3(poly_mesh.mesh->bmin),
                cell_size,
                cell_height](float const x, float const y, float const z)
        {
            return orig +
                glm::vec3{x * cell_size,
                    y * cell_height + 0.01f,
                    z * cell_size};
        };
        // NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay)

        main_draw_data_.max_triangles =
            cppext::narrow<uint32_t>(poly_mesh.mesh->npolys) *
            cppext::narrow<uint32_t>(vertices_per_polygon - 2) * 3;
        main_draw_data_.triangles = 0;

        main_draw_data_.max_lines =
            cppext::narrow<uint32_t>(poly_mesh.mesh->npolys) *
            cppext::narrow<uint32_t>(vertices_per_polygon) * 3;
        main_draw_data_.lines = 0;

        main_draw_data_.max_points =
            cppext::narrow<uint32_t>(poly_mesh.mesh->nverts);
        main_draw_data_.points = 0;

        destroy(&backend_->device(), &main_draw_data_.vertex_buffer);
        main_draw_data_.vertex_buffer = vkrndr::create_buffer(
            backend_->device(),
            {.size =
                    (main_draw_data_.max_triangles + main_draw_data_.max_lines +
                        main_draw_data_.max_points) *
                    sizeof(vertex_t),
                .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                .allocation_flags =
                    VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
                .required_memory_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT});

        auto vertex_map{vkrndr::map_memory(backend_->device(),
            main_draw_data_.vertex_buffer)};
        [[maybe_unused]] boost::scope::defer_guard const destroy_vtx{
            [this, map = &vertex_map]()
            { unmap_memory(backend_->device(), map); }};

        auto* const vertices{vertex_map.as<vertex_t>()};

        auto* triangle_vertices{vertices};
        auto* line_vertices{vertices + main_draw_data_.max_triangles};
        auto* point_vertices{line_vertices + main_draw_data_.max_lines};

        for (int i{}; i != poly_mesh.mesh->npolys; ++i)
        {
            unsigned short const* const p{
                &poly_mesh.mesh->polys[cppext::narrow<ptrdiff_t>(
                    i * vertices_per_polygon * 2)]};
            unsigned char const area{poly_mesh.mesh->areas[i]};

            glm::vec4 color;
            switch (area)
            {
            case RC_WALKABLE_AREA:
                color = glm::vec4{0.0f, 192.0f, 255.0f, 64.0f} / 255.0f;
                break;
            case RC_NULL_AREA:
                color = glm::vec4{0.0f, 0.0f, 0.0f, 64.0f} / 255.0f;
                break;
            default:
            {
                float const v{cppext::as_fp(area)};
                color = glm::vec4{v, v, v, 64.0f} / 255.0f;
                break;
            }
            }

            for (int j{2}; j != vertices_per_polygon; ++j)
            {
                if (p[j] == RC_MESH_NULL_IDX)
                {
                    break;
                }

                for (auto const vertex_index : {p[0], p[j - 1], p[j]})
                {
                    unsigned short const* const v{
                        &poly_mesh.mesh->verts[cppext::narrow<ptrdiff_t>(
                            vertex_index * 3)]};
                    triangle_vertices->position = make_pos(v[0], v[1], v[2]);
                    triangle_vertices->color = color;

                    ++triangle_vertices;
                    ++main_draw_data_.triangles;
                }
            }
        }

        glm::vec4 const line_color{1.0f,
            48.0f / 255.0f,
            64.0f / 255.0f,
            220.0f / 255.0f};
        for (int i{}; i != poly_mesh.mesh->npolys; ++i)
        {
            unsigned short const* const p{
                &poly_mesh.mesh->polys[cppext::narrow<ptrdiff_t>(
                    i * vertices_per_polygon * 2)]};
            for (int j{}; j != vertices_per_polygon; ++j)
            {
                if (p[j] == RC_MESH_NULL_IDX)
                {
                    break;
                }

                if ((p[vertices_per_polygon + j] & 0x8000) == 0)
                {
                    continue;
                }

                int const nj{(j + 1 >= vertices_per_polygon ||
                                 p[j + 1] == RC_MESH_NULL_IDX)
                        ? 0
                        : j + 1};

                auto col{line_color};
                if ((p[vertices_per_polygon + j] & 0xf) != 0xf)
                {
                    col = glm::vec4{1.0f, 1.0f, 1.0f, 0.5f};
                }

                for (auto const vertex_index : {p[j], p[nj]})
                {
                    unsigned short const* const v{
                        &poly_mesh.mesh->verts[cppext::narrow<ptrdiff_t>(
                            vertex_index * 3)]};
                    line_vertices->position = make_pos(v[0], v[1], v[2]);
                    line_vertices->color = col;

                    ++line_vertices;
                    ++main_draw_data_.lines;
                }
            }
        }

        glm::vec4 const point_color{0.0f, 0.0f, 0.0f, 220.0f / 255.0f};
        for (int i{}; i != poly_mesh.mesh->nverts; ++i)
        {
            unsigned short const* const v{
                &poly_mesh.mesh->verts[cppext::narrow<ptrdiff_t>(i * 3)]};
            point_vertices->position = make_pos(v[0], v[1], v[2]);
            point_vertices->color = point_color;

            ++point_vertices;
            ++main_draw_data_.points;
        }
    }

    // Update detail poly mesh
    {
        detail_draw_data_.max_triangles =
            cppext::narrow<uint32_t>(poly_mesh.detail_mesh->ntris) * 3;
        detail_draw_data_.triangles = 0;

        detail_draw_data_.max_lines =
            cppext::narrow<uint32_t>(poly_mesh.detail_mesh->ntris) * 6;
        detail_draw_data_.lines = 0;

        detail_draw_data_.max_points =
            cppext::narrow<uint32_t>(poly_mesh.mesh->nverts);
        detail_draw_data_.points = 0;

        destroy(&backend_->device(), &detail_draw_data_.vertex_buffer);
        detail_draw_data_.vertex_buffer = vkrndr::create_buffer(
            backend_->device(),
            {.size = (detail_draw_data_.max_triangles +
                         detail_draw_data_.max_lines +
                         detail_draw_data_.max_points) *
                    sizeof(vertex_t),
                .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                .allocation_flags =
                    VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
                .required_memory_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT});

        auto vertex_map{vkrndr::map_memory(backend_->device(),
            detail_draw_data_.vertex_buffer)};
        [[maybe_unused]] boost::scope::defer_guard const destroy_vtx{
            [this, map = &vertex_map]()
            { unmap_memory(backend_->device(), map); }};

        auto* const vertices{vertex_map.as<vertex_t>()};

        auto* triangle_vertices{vertices};
        auto* line_vertices{vertices + detail_draw_data_.max_triangles};
        auto* point_vertices{line_vertices + detail_draw_data_.max_lines};

        std::minstd_rand generator;

        for (int i{}; i != poly_mesh.detail_mesh->nmeshes; ++i)
        {
            unsigned int const* const m{&poly_mesh.detail_mesh
                    ->meshes[cppext::narrow<ptrdiff_t>(i * 4)]};
            unsigned int const bverts{m[0]};
            unsigned int const btris{m[2]};
            int const ntris{cppext::narrow<int>(m[3])};

            float const* const verts{&poly_mesh.detail_mesh
                    ->verts[cppext::narrow<ptrdiff_t>(bverts * 3)]};
            unsigned char const* const tris{&poly_mesh.detail_mesh
                    ->tris[cppext::narrow<ptrdiff_t>(btris * 4)]};

            float const v{std::generate_canonical<float, 10>(generator)};

            glm::vec4 const color{v, v, v, 0.5f};

            for (int j{}; j != ntris; ++j)
            {
                triangle_vertices[detail_draw_data_.triangles++] = {
                    .position = glm::make_vec3(&verts[cppext::narrow<ptrdiff_t>(
                        tris[cppext::narrow<ptrdiff_t>(j * 4) + 0] * 3)]),
                    .color = color};
                triangle_vertices[detail_draw_data_.triangles++] = {
                    .position = glm::make_vec3(&verts[cppext::narrow<ptrdiff_t>(
                        tris[cppext::narrow<ptrdiff_t>(j * 4) + 1] * 3)]),
                    .color = color};
                triangle_vertices[detail_draw_data_.triangles++] = {
                    .position = glm::make_vec3(&verts[cppext::narrow<ptrdiff_t>(
                        tris[cppext::narrow<ptrdiff_t>(j * 4) + 2] * 3)]),
                    .color = color};
            }
        }

        for (int i{}; i != poly_mesh.detail_mesh->nmeshes; ++i)
        {
            unsigned int const* const m{&poly_mesh.detail_mesh
                    ->meshes[cppext::narrow<ptrdiff_t>(i * 4)]};
            unsigned int const bverts{m[0]};
            unsigned int const btris{m[2]};
            int const ntris{cppext::narrow<int>(m[3])};

            float const* const verts{&poly_mesh.detail_mesh
                    ->verts[cppext::narrow<ptrdiff_t>(bverts * 3)]};
            unsigned char const* const tris{&poly_mesh.detail_mesh
                    ->tris[cppext::narrow<ptrdiff_t>(btris * 4)]};

            glm::vec4 const internal_color{0.0f, 0.0f, 1.0f, 128.0f / 255.0f};
            glm::vec4 const external_color{0.0f, 1.0f, 0.0f, 128.0f / 255.0f};

            for (int j{}; j != ntris; ++j)
            {
                unsigned char const* const t{
                    &tris[cppext::narrow<ptrdiff_t>(j * 4)]};

                for (int k{}, kp{2}; k != 3; kp = k++)
                {
                    auto const ef{cppext::narrow<unsigned char>(
                        (t[3] >> (kp * 2)) & 0x3)};
                    if (ef == 0)
                    {
                        // Internal edgejs
                        if (t[kp] < t[k])
                        {
                            line_vertices[detail_draw_data_.lines++] = {
                                .position = glm::make_vec3(
                                    &verts[cppext::narrow<ptrdiff_t>(
                                        t[cppext::narrow<ptrdiff_t>(kp)] * 3)]),
                                .color = internal_color};
                            line_vertices[detail_draw_data_.lines++] = {
                                .position = glm::make_vec3(
                                    &verts[cppext::narrow<ptrdiff_t>(
                                        t[cppext::narrow<ptrdiff_t>(k)] * 3)]),
                                .color = internal_color};
                        }
                    }
                    else
                    {
                        line_vertices[detail_draw_data_.lines++] = {
                            .position =
                                glm::make_vec3(&verts[cppext::narrow<ptrdiff_t>(
                                    t[cppext::narrow<ptrdiff_t>(kp)] * 3)]),
                            .color = external_color};
                        line_vertices[detail_draw_data_.lines++] = {
                            .position =
                                glm::make_vec3(&verts[cppext::narrow<ptrdiff_t>(
                                    t[cppext::narrow<ptrdiff_t>(k)] * 3)]),
                            .color = external_color};
                    }
                }
            }
        }

        glm::vec4 const point_color{0.0f, 0.0f, 0.0f, 220.0f / 255.0f};
        for (int i{}; i != poly_mesh.detail_mesh->nverts; ++i)
        {
            point_vertices[detail_draw_data_.points++] = {
                .position = glm::make_vec3(&poly_mesh.detail_mesh
                        ->verts[cppext::narrow<ptrdiff_t>(i * 3)]),
                .color = point_color};
        }
    }
}

void galileo::navmesh_debug_t::draw(VkCommandBuffer command_buffer,
    bool const draw_main_mesh,
    bool const draw_detail_mesh) const
{
    if (draw_main_mesh)
    {
        draw(command_buffer, main_draw_data_);
    }
    if (draw_detail_mesh)
    {
        draw(command_buffer, detail_draw_data_);
    }
}

void galileo::navmesh_debug_t::draw(VkCommandBuffer command_buffer,
    mesh_draw_data_t const& data) const
{
    if (data.triangles + data.lines + data.points == 0)
    {
        return;
    }

    VkDeviceSize const zero_offset{};
    vkCmdBindVertexBuffers(command_buffer,
        0,
        1,
        &data.vertex_buffer.buffer,
        &zero_offset);

    if (data.triangles)
    {
        vkrndr::bind_pipeline(command_buffer, triangle_pipeline_);

        vkCmdDraw(command_buffer, data.triangles, 1, 0, 0);
    }

    if (data.lines)
    {
        vkrndr::bind_pipeline(command_buffer, line_pipeline_);

        vkCmdSetLineWidth(command_buffer, 3.0f);

        vkCmdDraw(command_buffer, data.lines, 1, data.max_triangles, 0);
    }

    if (data.points)
    {
        vkrndr::bind_pipeline(command_buffer, point_pipeline_);

        vkCmdDraw(command_buffer,
            data.points,
            1,
            data.max_triangles + data.max_lines,
            0);
    }
}
