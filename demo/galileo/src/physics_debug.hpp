#ifndef GALILEO_PHYSICS_DEBUG_INCLUDED
#define GALILEO_PHYSICS_DEBUG_INCLUDED

#include <Jolt/Jolt.h> // IWYU pragma: keep

#include <Jolt/Core/Array.h>
#include <Jolt/Core/Color.h>
#include <Jolt/Core/Core.h>
#include <Jolt/Core/Memory.h>
#include <Jolt/Core/Reference.h>
#include <Jolt/Math/Real.h>
#include <Jolt/Renderer/DebugRenderer.h>

#include <atomic>
#include <cstdint>

// IWYU pragma: no_include <Jolt/Core/STLAllocator.h>
// IWYU pragma: no_include <string_view>

namespace ngngfx
{
    class camera_t;
} // namespace ngngfx

namespace galileo
{
    class batch_renderer_t;
} // namespace galileo

namespace galileo
{
    class [[nodiscard]] physics_debug_t final : public JPH::DebugRenderer
    {
    public:
        explicit physics_debug_t(batch_renderer_t& batch_renderer);

        physics_debug_t(physics_debug_t const&) = delete;

        physics_debug_t(physics_debug_t&&) noexcept = delete;

    public:
        ~physics_debug_t() override = default;

    public:
        void set_camera(ngngfx::camera_t const& camera);

    public:
        physics_debug_t& operator=(physics_debug_t const&) = delete;

        physics_debug_t& operator=(physics_debug_t&&) noexcept = delete;

    private:
        class [[nodiscard]] batch_impl_t final : public JPH::RefTargetVirtual
        {
        public:
            JPH_OVERRIDE_NEW_DELETE

            void AddRef() override { ++ref_count_; }

            void Release() override
            {
                if (--ref_count_ == 0)
                {
                    triangles.clear();
                    delete this;
                }
            }

            JPH::Array<Triangle> triangles;

        private:
            std::atomic<uint32_t> ref_count_ = 0;
        };

    private: // JPH::DebugRenderer overrides
        void DrawLine(JPH::RVec3Arg inFrom,
            JPH::RVec3Arg inTo,
            JPH::ColorArg inColor) override;

        void DrawTriangle(JPH::RVec3Arg inV1,
            JPH::RVec3Arg inV2,
            JPH::RVec3Arg inV3,
            JPH::ColorArg inColor,
            ECastShadow inCastShadow) override;

        Batch CreateTriangleBatch(Triangle const* inTriangles,
            int inTriangleCount) override;

        Batch CreateTriangleBatch(Vertex const* inVertices,
            int inVertexCount,
            JPH::uint32 const* inIndices,
            int inIndexCount) override;

        void DrawGeometry(JPH::RMat44Arg inModelMatrix,
            JPH::AABox const& inWorldSpaceBounds,
            float inLODScaleSq,
            JPH::ColorArg inModelColor,
            GeometryRef const& inGeometry,
            ECullMode inCullMode,
            ECastShadow inCastShadow,
            EDrawMode inDrawMode) override;

        void DrawText3D(JPH::RVec3Arg inPosition,
            JPH::string_view const& inString,
            JPH::ColorArg inColor,
            float inHeight) override;

    private:
        batch_renderer_t* batch_renderer_;
        ngngfx::camera_t const* camera_{nullptr};
    };
} // namespace galileo
#endif
