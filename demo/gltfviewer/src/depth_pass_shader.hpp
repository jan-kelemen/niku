#ifndef GLTFVIEWER_DEPTH_PASS_SHADER_INCLUDED
#define GLTFVIEWER_DEPTH_PASS_SHADER_INCLUDED

#include <vkrndr_pipeline.hpp>
#include <vkrndr_shader_module.hpp>

#include <volk.h>

#include <filesystem>

namespace vkrndr
{
    class backend_t;
} // namespace vkrndr

namespace gltfviewer
{
    class render_graph_t;
} // namespace gltfviewer

namespace gltfviewer
{
    class [[nodiscard]] depth_pass_shader_t final
    {
    public:
        explicit depth_pass_shader_t(vkrndr::backend_t& backend);

        depth_pass_shader_t(depth_pass_shader_t const&) = delete;

        depth_pass_shader_t(depth_pass_shader_t&&) noexcept = delete;

    public:
        ~depth_pass_shader_t();

    public:
        [[nodiscard]] VkPipelineLayout pipeline_layout() const;

        void load(render_graph_t const& graph,
            VkDescriptorSetLayout environment_layout,
            VkDescriptorSetLayout materials_layout,
            VkFormat depth_buffer_format);

        void draw(render_graph_t const& graph,
            VkCommandBuffer command_buffer) const;

    public:
        depth_pass_shader_t& operator=(depth_pass_shader_t const&) = delete;

        depth_pass_shader_t& operator=(depth_pass_shader_t&&) noexcept = delete;

    private:
        vkrndr::backend_t* backend_;

        std::filesystem::file_time_type vertex_write_time_;
        vkrndr::shader_module_t vertex_shader_;

        vkrndr::pipeline_t depth_pipeline_;
    };
} // namespace gltfviewer
#endif
