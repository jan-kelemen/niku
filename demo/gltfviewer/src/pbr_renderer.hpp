#ifndef GLTFVIEWER_PBR_RENDERER_INCLUDED
#define GLTFVIEWER_PBR_RENDERER_INCLUDED

#include <vkgltf_model.hpp>

#include <vkrndr_image.hpp>
#include <vkrndr_pipeline.hpp>
#include <vkrndr_shader_module.hpp>

#include <volk.h>

#include <cstdint>
#include <filesystem>

namespace vkrndr
{
    class backend_t;
} // namespace vkrndr

namespace gltfviewer
{
    class [[nodiscard]] pbr_renderer_t final
    {
    public:
        explicit pbr_renderer_t(vkrndr::backend_t& backend);

        pbr_renderer_t(pbr_renderer_t const&) = delete;

        pbr_renderer_t(pbr_renderer_t&&) noexcept = delete;

    public:
        ~pbr_renderer_t();

    public:
        [[nodiscard]] VkPipelineLayout pipeline_layout() const;

        void load(vkgltf::model_t&& model,
            VkDescriptorSetLayout environment_layout,
            VkDescriptorSetLayout materials_layout,
            VkDescriptorSetLayout graph_layout,
            VkFormat depth_buffer_format);

        void draw(VkCommandBuffer command_buffer,
            vkrndr::image_t const& color_image,
            vkrndr::image_t const& depth_buffer);

    public:
        pbr_renderer_t& operator=(pbr_renderer_t const&) = delete;

        pbr_renderer_t& operator=(pbr_renderer_t&&) noexcept = delete;

    private:
        vkrndr::backend_t* backend_;

        vkgltf::model_t model_;

        std::filesystem::file_time_type vertex_write_time_;
        vkrndr::shader_module_t vertex_shader_;

        std::filesystem::file_time_type fragment_write_time_;
        vkrndr::shader_module_t fragment_shader_;

        vkrndr::pipeline_t double_sided_pipeline_;
        vkrndr::pipeline_t culling_pipeline_;
        vkrndr::pipeline_t blending_pipeline_;

        uint32_t debug_{0};
    };
} // namespace gltfviewer
#endif
