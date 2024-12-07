#ifndef GALILEO_GBUFFER_SHADER_INCLUDED
#define GALILEO_GBUFFER_SHADER_INCLUDED

#include <vkrndr_buffer.hpp>
#include <vkrndr_pipeline.hpp>

#include <volk.h>

namespace vkrndr
{
    class backend_t;
    struct image_t;
} // namespace vkrndr

namespace galileo
{
    class gbuffer_t;
} // namespace galileo

namespace galileo
{
    class [[nodiscard]] gbuffer_shader_t final
    {
    public:
        explicit gbuffer_shader_t(vkrndr::backend_t& backend,
            VkDescriptorSetLayout frame_info_layout,
            VkDescriptorSetLayout graph_layout,
            VkFormat depth_buffer_format);

        gbuffer_shader_t(gbuffer_shader_t const&) = delete;

        gbuffer_shader_t(gbuffer_shader_t&&) noexcept = delete;

    public:
        ~gbuffer_shader_t();

    public:
        [[nodiscard]] VkPipelineLayout pipeline_layout() const;

        void draw(VkCommandBuffer command_buffer,
            gbuffer_t& gbuffer,
            vkrndr::image_t const& depth_buffer);

    public:
        gbuffer_shader_t& operator=(gbuffer_shader_t const&) = delete;

        gbuffer_shader_t& operator=(gbuffer_shader_t&&) noexcept = delete;

    private:
        vkrndr::backend_t* backend_;

        vkrndr::buffer_t vertex_buffer_;
        vkrndr::buffer_t index_buffer_;

        vkrndr::pipeline_t pipeline_;
    };
} // namespace galileo
#endif
