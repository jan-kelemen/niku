#ifndef GLTFVIEWER_PBR_RENDERER_INCLUDED
#define GLTFVIEWER_PBR_RENDERER_INCLUDED

#include <vulkan/vulkan_core.h>

#include <vkrndr_buffer.hpp>
#include <vkrndr_pipeline.hpp>

#include <cstdint>

namespace vkgltf
{
    struct model_t;
} // namespace vkgltf

namespace vkrndr
{
    class backend_t;
    struct image_t;
} // namespace vkrndr

namespace gltfviewer
{
    class [[nodiscard]] pbr_renderer_t final
    {
    public:
        pbr_renderer_t(vkrndr::backend_t* backend);

        pbr_renderer_t(pbr_renderer_t const&) = delete;

        pbr_renderer_t(pbr_renderer_t&&) noexcept = delete;

    public:
        ~pbr_renderer_t();

    public:
        void load_model(vkgltf::model_t const& model);

        void draw(VkCommandBuffer command_buffer,
            vkrndr::image_t const& target_image);

    public:
        pbr_renderer_t& operator=(pbr_renderer_t const&) = delete;

        pbr_renderer_t& operator=(pbr_renderer_t&&) noexcept = delete;

    private:
        vkrndr::backend_t* backend_;

        vkrndr::buffer_t vertex_buffer_;
        uint32_t vertex_count_{};
        vkrndr::buffer_t index_buffer_;
        uint32_t index_count_{};

        vkrndr::pipeline_t pipeline_;
    };
} // namespace gltfviewer
#endif
