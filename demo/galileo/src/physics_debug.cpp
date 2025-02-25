#include <physics_debug.hpp>

#include <batch_renderer.hpp>
#include <config.hpp>

#include <cppext_cycled_buffer.hpp>

#include <ngnphy_jolt_adapter.hpp>

#include <ngngfx_camera.hpp>

#include <vkglsl_shader_set.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_buffer.hpp>
#include <vkrndr_memory.hpp>
#include <vkrndr_pipeline.hpp>
#include <vkrndr_shader_module.hpp>

#include <boost/scope/defer.hpp>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <Jolt/Jolt.h> // IWYU pragma: keep
#include <Jolt/Math/Mat44.h>
#include <Jolt/Math/Vec3.h>

#include <vma_impl.hpp>

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

galileo::physics_debug_t::physics_debug_t(batch_renderer_t& batch_renderer)
    : batch_renderer_{&batch_renderer}
{
    Initialize();
}

void galileo::physics_debug_t::set_camera(ngngfx::camera_t const& camera)
{
    camera_ = &camera;
}

void galileo::physics_debug_t::DrawLine(JPH::RVec3Arg const inFrom,
    JPH::RVec3Arg const inTo,
    JPH::ColorArg const inColor)
{
    auto const color{ngnphy::to_glm(inColor.ToVec4())};
    batch_renderer_->add_line(
        {.position = ngnphy::to_glm(inFrom), .color = color},
        {.position = ngnphy::to_glm(inTo), .color = color});
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
