#ifndef GALILEO_GBUFFER_SHADER_INCLUDED
#define GALILEO_GBUFFER_SHADER_INCLUDED

#include <vkrndr_pipeline.hpp>

#include <volk.h>

namespace vkrndr
{
    class backend_t;
} // namespace vkrndr

namespace galileo
{
    class scene_graph_t;
} // namespace galileo

namespace galileo
{
    class [[nodiscard]] gbuffer_shader_t final
    {
    public:
        explicit gbuffer_shader_t(vkrndr::backend_t& backend,
            VkDescriptorSetLayout frame_info_layout,
            VkDescriptorSetLayout materials_layout,
            VkDescriptorSetLayout graph_layout,
            VkFormat depth_buffer_format);

        gbuffer_shader_t(gbuffer_shader_t const&) = delete;

        gbuffer_shader_t(gbuffer_shader_t&&) noexcept = delete;

    public:
        ~gbuffer_shader_t();

    public:
        [[nodiscard]] VkPipelineLayout pipeline_layout() const;

        void draw(scene_graph_t& graph, VkCommandBuffer command_buffer) const;

    public:
        gbuffer_shader_t& operator=(gbuffer_shader_t const&) = delete;

        gbuffer_shader_t& operator=(gbuffer_shader_t&&) noexcept = delete;

    private:
        vkrndr::backend_t* backend_;

        vkrndr::pipeline_layout_t pipeline_layout_;
        vkrndr::pipeline_t pipeline_;
    };
} // namespace galileo
#endif
