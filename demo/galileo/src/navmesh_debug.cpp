#include <navmesh_debug.hpp>

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

    void allocate_buffer(vkrndr::device_t const& device,
        galileo::detail::mesh_draw_data_t& data)
    {
        data.vertex_buffer = create_buffer(device,
            {.size = (data.max_triangles + data.max_lines + data.max_points) *
                    sizeof(vertex_t),
                .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                .allocation_flags =
                    VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
                .required_memory_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT});
    }

    galileo::detail::mesh_draw_data_t create_main_mesh_draw_data(
        vkrndr::device_t const& device,
        rcPolyMesh const& mesh)
    {
        int const vertices_per_polygon{mesh.nvp};

        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
        auto const make_pos = [orig = glm::make_vec3(mesh.bmin),
                                  cell_size = mesh.cs,
                                  cell_height = mesh.ch](float const x,
                                  float const y,
                                  float const z)
        {
            return orig +
                glm::vec3{x * cell_size,
                    y * cell_height + 0.01f,
                    z * cell_size};
        };
        // NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay)

        galileo::detail::mesh_draw_data_t rv{
            .max_triangles = cppext::narrow<uint32_t>(mesh.npolys) *
                cppext::narrow<uint32_t>(vertices_per_polygon - 2) * 3,
            .max_lines = cppext::narrow<uint32_t>(mesh.npolys) *
                cppext::narrow<uint32_t>(vertices_per_polygon) * 3,
            .max_points = cppext::narrow<uint32_t>(mesh.nverts)};

        allocate_buffer(device, rv);
        boost::scope::scope_fail const rollback{
            [&device, &rv]() { destroy(&device, &rv.vertex_buffer); }};

        auto vertex_map{vkrndr::map_memory(device, rv.vertex_buffer)};
        [[maybe_unused]] boost::scope::defer_guard const destroy_vtx{
            [device, map = &vertex_map]() { unmap_memory(device, map); }};

        auto* const vertices{vertex_map.as<vertex_t>()};

        auto* const triangle_vertices{vertices};
        auto* const line_vertices{vertices + rv.max_triangles};
        auto* const point_vertices{line_vertices + rv.max_lines};

        for (int i{}; i != mesh.npolys; ++i)
        {
            unsigned short const* const polygon{
                &mesh.polys[cppext::narrow<ptrdiff_t>(
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
                    }(mesh.areas[i]) /
                    255.0f};

            for (int j{2}; j != vertices_per_polygon; ++j)
            {
                if (polygon[j] == RC_MESH_NULL_IDX)
                {
                    break;
                }

                for (auto const vertex_index :
                    {polygon[0], polygon[j - 1], polygon[j]})
                {
                    unsigned short const* const v{
                        &mesh.verts[cppext::narrow<ptrdiff_t>(
                            vertex_index * 3)]};
                    triangle_vertices[rv.triangles++] = {
                        .position = make_pos(v[0], v[1], v[2]),
                        .color = color};
                }
            }
        }

        glm::vec4 const line_color{1.0f,
            48.0f / 255.0f,
            64.0f / 255.0f,
            220.0f / 255.0f};
        for (int i{}; i != mesh.npolys; ++i)
        {
            unsigned short const* const polygon{
                &mesh.polys[cppext::narrow<ptrdiff_t>(
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

                for (auto const vertex_index : {polygon[j], polygon[nj]})
                {
                    unsigned short const* const v{
                        &mesh.verts[cppext::narrow<ptrdiff_t>(
                            vertex_index * 3)]};
                    line_vertices[rv.lines++] = {
                        .position = make_pos(v[0], v[1], v[2]),
                        .color = col};
                }
            }
        }

        constexpr glm::vec4 point_color{0.0f, 0.0f, 0.0f, 220.0f / 255.0f};
        auto const nverts{cppext::narrow<ptrdiff_t>(mesh.nverts)};
        for (ptrdiff_t i{}; i != nverts; ++i)
        {
            unsigned short const* const v{&mesh.verts[i * 3]};
            point_vertices[rv.points++] = {
                .position = make_pos(v[0], v[1], v[2]),
                .color = point_color};
        }

        return rv;
    }

    galileo::detail::mesh_draw_data_t create_detail_mesh_draw_data(
        vkrndr::device_t const& device,
        rcPolyMeshDetail const& mesh)
    {
        galileo::detail::mesh_draw_data_t rv{
            .max_triangles = cppext::narrow<uint32_t>(mesh.ntris) * 3,
            .max_lines = cppext::narrow<uint32_t>(mesh.ntris) * 6,
            .max_points = cppext::narrow<uint32_t>(mesh.nverts)};

        allocate_buffer(device, rv);
        boost::scope::scope_fail const rollback{
            [&device, &rv]() { destroy(&device, &rv.vertex_buffer); }};

        auto vertex_map{vkrndr::map_memory(device, rv.vertex_buffer)};
        [[maybe_unused]] boost::scope::defer_guard const destroy_vtx{
            [&device, map = &vertex_map]() { unmap_memory(device, map); }};

        auto* const vertices{vertex_map.as<vertex_t>()};

        auto* const triangle_vertices{vertices};
        auto* const line_vertices{vertices + rv.max_triangles};
        auto* const point_vertices{line_vertices + rv.max_lines};

        std::minstd_rand generator;

        for (int i{}; i != mesh.nmeshes; ++i)
        {
            unsigned int const* const m{
                &mesh.meshes[cppext::narrow<ptrdiff_t>(i * 4)]};
            unsigned int const bverts{m[0]};
            unsigned int const btris{m[2]};

            float const* const verts{
                &mesh.verts[cppext::narrow<ptrdiff_t>(bverts * 3)]};
            unsigned char const* const tris{
                &mesh.tris[cppext::narrow<ptrdiff_t>(btris * 4)]};

            float const v{std::generate_canonical<float, 10>(generator)};
            glm::vec4 const color{v, v, v, 0.5f};

            auto const ntris{cppext::narrow<ptrdiff_t>(m[3])};
            for (ptrdiff_t j{}; j != ntris; ++j)
            {
                triangle_vertices[rv.triangles++] = {
                    .position = glm::make_vec3(
                        &verts[cppext::narrow<ptrdiff_t>(tris[j * 4 + 0] * 3)]),
                    .color = color};
                triangle_vertices[rv.triangles++] = {
                    .position = glm::make_vec3(
                        &verts[cppext::narrow<ptrdiff_t>(tris[j * 4 + 1] * 3)]),
                    .color = color};
                triangle_vertices[rv.triangles++] = {
                    .position = glm::make_vec3(
                        &verts[cppext::narrow<ptrdiff_t>(tris[j * 4 + 2] * 3)]),
                    .color = color};
            }
        }

        for (int i{}; i != mesh.nmeshes; ++i)
        {
            unsigned int const* const m{
                &mesh.meshes[cppext::narrow<ptrdiff_t>(i * 4)]};
            unsigned int const bverts{m[0]};
            unsigned int const btris{m[2]};

            float const* const verts{
                &mesh.verts[cppext::narrow<ptrdiff_t>(bverts * 3)]};
            unsigned char const* const tris{
                &mesh.tris[cppext::narrow<ptrdiff_t>(btris * 4)]};

            auto const ntris{cppext::narrow<ptrdiff_t>(m[3])};
            for (ptrdiff_t j{}; j != ntris; ++j)
            {
                unsigned char const* const t{&tris[j * 4]};

                for (ptrdiff_t k{}, kp{2}; k != 3; kp = k++)
                {
                    auto const ef{cppext::narrow<unsigned char>(
                        (t[3] >> (kp * 2)) & 0x3)};
                    if (ef == 0)
                    {
                        // Internal edges
                        if (t[kp] < t[k])
                        {
                            constexpr glm::vec4 internal_color{0.0f,
                                0.0f,
                                1.0f,
                                0.5f};
                            line_vertices[rv.lines++] = {
                                .position = glm::make_vec3(
                                    &verts[cppext::narrow<ptrdiff_t>(
                                        t[kp] * 3)]),
                                .color = internal_color};
                            line_vertices[rv.lines++] = {
                                .position = glm::make_vec3(
                                    &verts[cppext::narrow<ptrdiff_t>(
                                        t[k] * 3)]),
                                .color = internal_color};
                        }
                    }
                    else
                    {
                        // External edges
                        constexpr glm::vec4 external_color{0.0f,
                            1.0f,
                            0.0f,
                            0.5f};
                        line_vertices[rv.lines++] = {
                            .position = glm::make_vec3(
                                &verts[cppext::narrow<ptrdiff_t>(t[kp] * 3)]),
                            .color = external_color};
                        line_vertices[rv.lines++] = {
                            .position = glm::make_vec3(
                                &verts[cppext::narrow<ptrdiff_t>(t[k] * 3)]),
                            .color = external_color};
                    }
                }
            }
        }

        constexpr glm::vec4 point_color{0.0f, 0.0f, 0.0f, 220.0f / 255.0f};
        auto const nverts{cppext::narrow<ptrdiff_t>(mesh.nverts)};
        for (ptrdiff_t i{}; i != nverts; ++i)
        {
            point_vertices[rv.points++] = {
                .position = glm::make_vec3(&mesh.verts[i * 3]),
                .color = point_color};
        }

        return rv;
    }

    galileo::detail::mesh_draw_data_t create_path_draw_data(
        vkrndr::device_t const& device,
        std::span<glm::vec3 const> const& path_points)
    {
        galileo::detail::mesh_draw_data_t rv{
            .max_lines = cppext::narrow<uint32_t>(path_points.size()),
            .max_points = cppext::narrow<uint32_t>(path_points.size())};

        allocate_buffer(device, rv);
        boost::scope::scope_fail const rollback{
            [&device, &rv]() { destroy(&device, &rv.vertex_buffer); }};

        auto vertex_map{vkrndr::map_memory(device, rv.vertex_buffer)};
        [[maybe_unused]] boost::scope::defer_guard const destroy_vtx{
            [device, map = &vertex_map]() { unmap_memory(device, map); }};

        auto* const vertices{vertex_map.as<vertex_t>()};

        auto* const line_vertices{vertices + rv.max_triangles};
        auto* const point_vertices{line_vertices + rv.max_lines};

        for (auto const& point : path_points)
        {
            line_vertices[rv.lines++] = {.position = point,
                .color = glm::vec4{1.0f, 1.0f, 0.0f, 0.6f}};

            point_vertices[rv.points++] = {.position = point,
                .color = glm::vec4{1.0f, 1.0f, 0.0f, 0.8f}};
        }

        return rv;
    }
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

    destroy(&backend_->device(), &path_draw_data_.vertex_buffer);
    destroy(&backend_->device(), &detail_draw_data_.vertex_buffer);
    destroy(&backend_->device(), &main_draw_data_.vertex_buffer);
}

VkPipelineLayout galileo::navmesh_debug_t::pipeline_layout() const
{
    return *triangle_pipeline_.layout;
}

void galileo::navmesh_debug_t::update(poly_mesh_t const& poly_mesh)
{
    destroy(&backend_->device(), &main_draw_data_.vertex_buffer);
    main_draw_data_ =
        create_main_mesh_draw_data(backend_->device(), *poly_mesh.mesh);

    destroy(&backend_->device(), &detail_draw_data_.vertex_buffer);
    detail_draw_data_ = create_detail_mesh_draw_data(backend_->device(),
        *poly_mesh.detail_mesh);
}

void galileo::navmesh_debug_t::update(
    std::span<glm::vec3 const> const& path_points)
{
    destroy(&backend_->device(), &path_draw_data_.vertex_buffer);
    path_draw_data_ = create_path_draw_data(backend_->device(), path_points);
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

    draw(command_buffer, path_draw_data_);
}

void galileo::navmesh_debug_t::draw(VkCommandBuffer command_buffer,
    detail::mesh_draw_data_t const& data) const
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
