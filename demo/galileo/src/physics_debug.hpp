#ifndef GALILEO_PHYSICS_DEBUG_INCLUDED
#define GALILEO_PHYSICS_DEBUG_INCLUDED

#include <cppext_cycled_buffer.hpp>

#include <vkrndr_buffer.hpp>
#include <vkrndr_memory.hpp>
#include <vkrndr_pipeline.hpp>

#include <Jolt/Jolt.h> // IWYU pragma: keep

#include <Jolt/Core/Array.h>
#include <Jolt/Core/Color.h>
#include <Jolt/Core/Core.h>
#include <Jolt/Core/Memory.h>
#include <Jolt/Core/Reference.h>
#include <Jolt/Math/Real.h>
#include <Jolt/Renderer/DebugRenderer.h>

#include <volk.h>

#include <atomic>
#include <cstdint>

// IWYU pragma: no_include <Jolt/Core/STLAllocator.h>
// IWYU pragma: no_include <string_view>

namespace niku
{
    class camera_t;
} // namespace niku

namespace vkrndr
{
    class backend_t;
    struct image_t;
} // namespace vkrndr

namespace galileo
{
    class [[nodiscard]] physics_debug_t final : public JPH::DebugRenderer
    {
    public:
        explicit physics_debug_t(vkrndr::backend_t& backend);

        physics_debug_t(physics_debug_t const&) = delete;

        physics_debug_t(physics_debug_t&&) noexcept = delete;

    public:
        ~physics_debug_t();

    public:
        void set_camera(niku::camera_t const& camera);

        void draw(VkCommandBuffer command_buffer,
            vkrndr::image_t const& target_image);

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
                }
            }

            JPH::Array<Triangle> triangles;

        private:
            std::atomic<uint32_t> ref_count_ = 0;
        };

        struct [[nodiscard]] frame_data_t final
        {
            vkrndr::buffer_t vertex_buffer;
            vkrndr::mapped_memory_t vertex_map;
            uint32_t vertex_count{};
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
        vkrndr::backend_t* backend_;
        niku::camera_t const* camera_{nullptr};

        vkrndr::pipeline_t line_pipeline_;

        cppext::cycled_buffer_t<frame_data_t> frame_data_;
    };
} // namespace galileo
#endif
