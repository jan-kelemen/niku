#include <navmesh_debug.hpp>

#include <config.hpp>

#include <vkglsl_shader_set.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_memory.hpp>

#include <boost/scope/defer.hpp>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <recastnavigation/Recast.h>

#include <volk.h>

#include <array>

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

    constexpr size_t max_line_count{50000};
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

    pipeline_ =
        vkrndr::pipeline_builder_t{backend_->device(),
            vkrndr::pipeline_layout_builder_t{backend_->device()}
                .add_descriptor_set_layout(frame_info_layout)
                .build()}
            .add_shader(as_pipeline_shader(*vertex_shader))
            .add_shader(as_pipeline_shader(*fragment_shader))
            .add_color_attachment(VK_FORMAT_R16G16B16A16_SFLOAT, alpha_blend)
            .with_depth_test(depth_buffer_format)
            .add_vertex_input(binding_description(), attribute_description())
            .build();
}

galileo::navmesh_debug_t::~navmesh_debug_t()
{
    destroy(&backend_->device(), &pipeline_);

    destroy(&backend_->device(), &vertex_buffer_);
}

VkPipelineLayout galileo::navmesh_debug_t::pipeline_layout()
{
    return *pipeline_.layout;
}

void galileo::navmesh_debug_t::update(rcPolyMesh const& poly_mesh)
{
    int const vertices_per_polygon{poly_mesh.nvp};
    float const cell_size{poly_mesh.cs};
    float const cell_height{poly_mesh.ch};
    float const* orig{poly_mesh.bmin};

    vertex_count_ = 0;

    destroy(&backend_->device(), &vertex_buffer_);
    vertex_buffer_ = vkrndr::create_buffer(backend_->device(),
        {.size = cppext::narrow<size_t>(poly_mesh.npolys) *
                cppext::narrow<size_t>(vertices_per_polygon) * 3 *
                sizeof(vertex_t),
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            .allocation_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
            .required_memory_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT});

    auto vertex_map{vkrndr::map_memory(backend_->device(), vertex_buffer_)};
    [[maybe_unused]] boost::scope::defer_guard const destroy_vtx{
        [this, map = &vertex_map]() { unmap_memory(backend_->device(), map); }};

    auto* const vertices{vertex_map.as<vertex_t>()};

    for (int i{}; i != poly_mesh.npolys; ++i)
    {
        unsigned short const* p{&poly_mesh.polys[i * vertices_per_polygon * 2]};
        unsigned char const area{poly_mesh.areas[i]};

        glm::vec4 color;
        switch (area)
        {
        case RC_WALKABLE_AREA:
            color = glm::vec4{0.0f, 192.0f / 255.0f, 1.0f, 64.0f / 255.0f};
            break;
        case RC_NULL_AREA:
            color = glm::vec4{0.0f, 0.0f, 0.0f, 64.0f / 255.0f};
            break;
        default:
        {
            float const v{cppext::as_fp(area) / 255.0f};
            color = glm::vec4{v, v, v, 64.0f / 255.0f};
            break;
        }
        }

        for (int j{2}; j < vertices_per_polygon; ++j)
        {
            if (p[j] == RC_MESH_NULL_IDX)
                break;

            std::array const vi{p[0], p[j - 1], p[j]};
            for (int k = 0; k < 3; ++k)
            {
                unsigned short const* v{&poly_mesh.verts[vi[k] * 3]};
                float const x{orig[0] + v[0] * cell_size};
                float const y{orig[1] + (v[1] + 0.01f) * cell_height};
                float const z{orig[2] + v[2] * cell_size};

                vertices[vertex_count_].position = glm::vec3{x, y, z};
                vertices[vertex_count_].color = color;

                ++vertex_count_;
            }
        }
    }
}

void galileo::navmesh_debug_t::draw(VkCommandBuffer command_buffer)
{
    if (vertex_count_ == 0)
    {
        return;
    }

    VkDeviceSize const zero_offset{};
    vkCmdBindVertexBuffers(command_buffer,
        0,
        1,
        &vertex_buffer_.buffer,
        &zero_offset);

    vkrndr::bind_pipeline(command_buffer, pipeline_);

    vkCmdDraw(command_buffer, vertex_count_, 1, 0, 0);
}
