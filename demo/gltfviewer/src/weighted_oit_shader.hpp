#ifndef GLTFVIEWER_WEIGHTED_OIT_SHADER_INCLUDED
#define GLTFVIEWER_WEIGHTED_OIT_SHADER_INCLUDED

#include <vkrndr_image.hpp>
#include <vkrndr_pipeline.hpp>

#include <volk.h>

#include <cstdint>

namespace vkrndr
{
    class backend_t;
} // namespace vkrndr

namespace gltfviewer
{
    class scene_graph_t;
} // namespace gltfviewer

namespace gltfviewer
{
    class [[nodiscard]] weighted_oit_shader_t final
    {
    public:
        explicit weighted_oit_shader_t(vkrndr::backend_t& backend);

        weighted_oit_shader_t(weighted_oit_shader_t const&) = delete;

        weighted_oit_shader_t(weighted_oit_shader_t&&) noexcept = delete;

    public:
        ~weighted_oit_shader_t();

    public:
        [[nodiscard]] VkPipelineLayout pipeline_layout() const;

        void load(scene_graph_t const& graph,
            VkDescriptorSetLayout environment_layout,
            VkDescriptorSetLayout materials_layout,
            VkDescriptorSetLayout shadow_layout,
            VkFormat depth_buffer_format);

        void draw(scene_graph_t const& graph,
            VkCommandBuffer command_buffer,
            vkrndr::image_t const& color_image,
            vkrndr::image_t const& depth_buffer);

        void resize(uint32_t width, uint32_t height);

    public:
        weighted_oit_shader_t& operator=(weighted_oit_shader_t const&) = delete;

        weighted_oit_shader_t& operator=(
            weighted_oit_shader_t&&) noexcept = delete;

    private:
        vkrndr::backend_t* backend_;

        vkrndr::image_t accumulation_image_;
        vkrndr::image_t reveal_image_;

        VkDescriptorSetLayout descriptor_set_layout_;
        VkDescriptorSet descriptor_set_{VK_NULL_HANDLE};
        VkSampler accumulation_sampler_;
        VkSampler reveal_sampler_;

        vkrndr::pipeline_t pbr_pipeline_;
        vkrndr::pipeline_t composition_pipeline_;
    };
} // namespace gltfviewer
#endif
