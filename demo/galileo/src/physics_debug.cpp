#include <physics_debug.hpp>

#include <cppext_cycled_buffer.hpp>

#include <ngnphy_jolt_adapter.hpp>

#include <ngngfx_camera.hpp>

#include <vkglsl_shader_set.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_buffer.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_memory.hpp>
#include <vkrndr_pipeline.hpp>
#include <vkrndr_render_pass.hpp>
#include <vkrndr_shader_module.hpp>

#include <boost/scope/defer.hpp>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <Jolt/Jolt.h> // IWYU pragma: keep
#include <Jolt/Math/Mat44.h>
#include <Jolt/Math/Vec3.h>

#include <spdlog/spdlog.h>

#include <array>
#include <cassert>
#include <cstddef>
#include <memory>
#include <span>

// IWYU pragma: no_include <fmt/base.h>
// IWYU pragma: no_include <fmt/format.h>
// IWYU pragma: no_include <glm/detail/qualifier.hpp>
// IWYU pragma: no_include <expected>
// IWYU pragma: no_include <filesystem>
// IWYU pragma: no_include <iterator>
// IWYU pragma: no_include <new>
// IWYU pragma: no_include <optional>
// IWYU pragma: no_include <system_error>

namespace
{
    struct [[nodiscard]] line_vertex_t final
    {
        glm::vec3 position;
        char padding;
        glm::vec4 color;
    };

    consteval auto binding_description()
    {
        constexpr std::array descriptions{
            VkVertexInputBindingDescription{.binding = 0,
                .stride = sizeof(line_vertex_t),
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
                .offset = offsetof(line_vertex_t, position)},
            VkVertexInputAttributeDescription{.location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                .offset = offsetof(line_vertex_t, color)},
        };

        return descriptions;
    }

    constexpr size_t max_line_count{50000};
} // namespace

galileo::physics_debug_t::physics_debug_t(vkrndr::backend_t& backend,
    VkDescriptorSetLayout frame_info_layout,
    VkFormat depth_buffer_format)
    : backend_{&backend}
    , frame_data_{backend_->frames_in_flight(), backend_->frames_in_flight()}
{
    Initialize();

    vkglsl::shader_set_t shader_set{true, false};

    auto vertex_shader{add_shader_module_from_path(shader_set,
        backend_->device(),
        VK_SHADER_STAGE_VERTEX_BIT,
        "physics_debug_line.vert")};
    assert(vertex_shader);
    [[maybe_unused]] boost::scope::defer_guard const destroy_vtx{
        [this, shd = &vertex_shader.value()]()
        { destroy(&backend_->device(), shd); }};

    auto fragment_shader{add_shader_module_from_path(shader_set,
        backend_->device(),
        VK_SHADER_STAGE_FRAGMENT_BIT,
        "physics_debug_line.frag")};
    assert(fragment_shader);
    [[maybe_unused]] boost::scope::defer_guard const destroy_frag{
        [this, shd = &fragment_shader.value()]()
        { destroy(&backend_->device(), shd); }};

    line_pipeline_ =
        vkrndr::pipeline_builder_t{backend_->device(),
            vkrndr::pipeline_layout_builder_t{backend_->device()}
                .add_descriptor_set_layout(frame_info_layout)
                .build()}
            .with_primitive_topology(VK_PRIMITIVE_TOPOLOGY_LINE_LIST)
            .add_shader(as_pipeline_shader(*vertex_shader))
            .add_shader(as_pipeline_shader(*fragment_shader))
            .add_color_attachment(VK_FORMAT_R16G16B16A16_SFLOAT)
            .with_depth_test(depth_buffer_format)
            .add_vertex_input(binding_description(), attribute_description())
            .build();

    for (frame_data_t& data : frame_data_.as_span())
    {
        data.vertex_buffer = vkrndr::create_buffer(backend_->device(),
            max_line_count * sizeof(line_vertex_t),
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        data.vertex_map =
            vkrndr::map_memory(backend_->device(), data.vertex_buffer);
    }
}

galileo::physics_debug_t::~physics_debug_t()
{
    for (frame_data_t& data : frame_data_.as_span())
    {
        unmap_memory(backend_->device(), &data.vertex_map);
        destroy(&backend_->device(), &data.vertex_buffer);
    }

    destroy(&backend_->device(), &line_pipeline_);
}

VkPipelineLayout galileo::physics_debug_t::pipeline_layout()
{
    return *line_pipeline_.layout;
}

void galileo::physics_debug_t::set_camera(ngngfx::camera_t const& camera)
{
    camera_ = &camera;
}

void galileo::physics_debug_t::draw(VkCommandBuffer command_buffer,
    vkrndr::image_t const& target_image,
    vkrndr::image_t const& depth_buffer)
{
    [[maybe_unused]] boost::scope::defer_guard const on_exit{[this]()
        {
            frame_data_.cycle(
                [](auto& current, auto&) { current.vertex_count = 0; });
        }};

    if (frame_data_->vertex_count == 0)
    {
        return;
    }

    VkDeviceSize const zero_offset{};
    vkCmdBindVertexBuffers(command_buffer,
        0,
        1,
        &frame_data_->vertex_buffer.buffer,
        &zero_offset);

    vkrndr::render_pass_t render_pass;
    render_pass.with_color_attachment(VK_ATTACHMENT_LOAD_OP_LOAD,
        VK_ATTACHMENT_STORE_OP_STORE,
        target_image.view);
    render_pass.with_depth_attachment(VK_ATTACHMENT_LOAD_OP_LOAD,
        VK_ATTACHMENT_STORE_OP_NONE,
        depth_buffer.view);

    {
        [[maybe_unused]] auto const guard{
            render_pass.begin(command_buffer, {{0, 0}, target_image.extent})};

        vkrndr::bind_pipeline(command_buffer, line_pipeline_);

        vkCmdDraw(command_buffer, frame_data_->vertex_count, 1, 0, 0);
    }
}

void galileo::physics_debug_t::DrawLine(JPH::RVec3Arg const inFrom,
    JPH::RVec3Arg const inTo,
    JPH::ColorArg const inColor)
{
    auto* const lines{frame_data_->vertex_map.as<line_vertex_t>()};
    if (frame_data_->vertex_count + 2 <= max_line_count)
    {
        auto const color{ngnphy::to_glm(inColor.ToVec4())};

        line_vertex_t& first{lines[frame_data_->vertex_count++]};
        first.position = ngnphy::to_glm(inFrom);
        first.color = color;

        line_vertex_t& second{lines[frame_data_->vertex_count++]};
        second.position = ngnphy::to_glm(inTo);
        second.color = color;
    }
}

void galileo::physics_debug_t::DrawTriangle(JPH::RVec3Arg inV1,
    JPH::RVec3Arg const inV2,
    JPH::RVec3Arg const inV3,
    JPH::ColorArg const inColor,
    [[maybe_unused]] ECastShadow const inCastShadow)
{
    DrawLine(inV1, inV2, inColor);
    DrawLine(inV2, inV3, inColor);
    DrawLine(inV3, inV1, inColor);
}

JPH::DebugRenderer::Batch galileo::physics_debug_t::CreateTriangleBatch(
    Triangle const* const inTriangles,
    int const inTriangleCount)
{
    auto batch{std::make_unique<batch_impl_t>()};
    if (!inTriangles || inTriangleCount <= 0)
    {
        return batch.release();
    }

    batch->triangles.assign(inTriangles, inTriangles + inTriangleCount);
    return batch.release();
}

JPH::DebugRenderer::Batch galileo::physics_debug_t::CreateTriangleBatch(
    Vertex const* const inVertices,
    int const inVertexCount,
    JPH::uint32 const* const inIndices,
    int const inIndexCount)
{
    auto batch{std::make_unique<batch_impl_t>()};
    if (!inVertices || inVertexCount <= 0 || !inIndices || inIndexCount <= 0)
    {
        return batch.release();
    }

    // Convert indexed triangle list to triangle list
    batch->triangles.resize(static_cast<size_t>(inIndexCount) / 3);
    for (size_t t{}; t < batch->triangles.size(); ++t)
    {
        Triangle& triangle{batch->triangles[t]};
        triangle.mV[0] = inVertices[inIndices[t * 3 + 0]];
        triangle.mV[1] = inVertices[inIndices[t * 3 + 1]];
        triangle.mV[2] = inVertices[inIndices[t * 3 + 2]];
    }

    return batch.release();
}

void galileo::physics_debug_t::DrawGeometry(JPH::RMat44Arg inModelMatrix,
    JPH::AABox const& inWorldSpaceBounds,
    float const inLODScaleSq,
    JPH::ColorArg const inModelColor,
    GeometryRef const& inGeometry,
    [[maybe_unused]] ECullMode const inCullMode,
    [[maybe_unused]] ECastShadow const inCastShadow,
    EDrawMode const inDrawMode)
{
    // Figure out which LOD to use
    LOD const* lod = inGeometry->mLODs.data();
    if (camera_)
    {
        auto const& position{camera_->position()};
        lod = &inGeometry->GetLOD(JPH::Vec3{position.x, position.y, position.z},
            inWorldSpaceBounds,
            inLODScaleSq);
    }

    // NOLINTBEGIN(cppcoreguidelines-pro-type-static-cast-downcast)
    batch_impl_t const* const batch{
        static_cast<batch_impl_t const*>(lod->mTriangleBatch.GetPtr())};
    // NOLINTEND(cppcoreguidelines-pro-type-static-cast-downcast)

    for (Triangle const& triangle : batch->triangles)
    {
        JPH::RVec3 const v0{
            inModelMatrix * JPH::Vec3{triangle.mV[0].mPosition}};
        JPH::RVec3 const v1{
            inModelMatrix * JPH::Vec3{triangle.mV[1].mPosition}};
        JPH::RVec3 const v2{
            inModelMatrix * JPH::Vec3{triangle.mV[2].mPosition}};
        JPH::Color const color{inModelColor * triangle.mV[0].mColor};

        switch (inDrawMode)
        {
        case EDrawMode::Wireframe:
            DrawLine(v0, v1, color);
            DrawLine(v1, v2, color);
            DrawLine(v2, v0, color);
            break;

        case EDrawMode::Solid:
            DrawTriangle(v0, v1, v2, color, inCastShadow);
            break;
        }
    }
}

void galileo::physics_debug_t::DrawText3D(
    [[maybe_unused]] JPH::RVec3Arg const inPosition,
    JPH::string_view const& inString,
    [[maybe_unused]] JPH::ColorArg const inColor,
    [[maybe_unused]] float const inHeight)
{
    spdlog::info("{}", inString);
}
