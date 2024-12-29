#ifndef GLTFVIEWER_PBR_SHADER_INCLUDED
#define GLTFVIEWER_PBR_SHADER_INCLUDED

#include <vkrndr_pipeline.hpp>
#include <vkrndr_shader_module.hpp>

#include <volk.h>

#include <filesystem>

namespace vkrndr
{
    class backend_t;
    struct image_t;
} // namespace vkrndr

namespace gltfviewer
{
    class render_graph_t;
} // namespace gltfviewer

namespace gltfviewer
{
    class [[nodiscard]] pbr_shader_t final
    {
    public:
        explicit pbr_shader_t(vkrndr::backend_t& backend);

        pbr_shader_t(pbr_shader_t const&) = delete;

        pbr_shader_t(pbr_shader_t&&) noexcept = delete;

    public:
        ~pbr_shader_t();

    public:
        [[nodiscard]] VkPipelineLayout pipeline_layout() const;

        void load(render_graph_t const& graph,
            VkDescriptorSetLayout environment_layout,
            VkDescriptorSetLayout materials_layout,
            VkFormat depth_buffer_format);

        void draw(render_graph_t const& graph,
            VkCommandBuffer command_buffer,
            vkrndr::image_t const& color_image,
            vkrndr::image_t const& depth_buffer);

    public:
        pbr_shader_t& operator=(pbr_shader_t const&) = delete;

        pbr_shader_t& operator=(pbr_shader_t&&) noexcept = delete;

    private:
        vkrndr::backend_t* backend_;

        std::filesystem::file_time_type vertex_write_time_;
        vkrndr::shader_module_t vertex_shader_;

        std::filesystem::file_time_type fragment_write_time_;
        vkrndr::shader_module_t fragment_shader_;

        vkrndr::pipeline_t double_sided_pipeline_;
        vkrndr::pipeline_t culling_pipeline_;
        vkrndr::pipeline_t depth_pipeline_;
    };
} // namespace gltfviewer
#endif
