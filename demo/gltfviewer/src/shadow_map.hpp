#ifndef GLTFVIEWER_SHADOW_MAP_INCLUDED
#define GLTFVIEWER_SHADOW_MAP_INCLUDED

#include <vkrndr_image.hpp>
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
    class scene_graph_t;
} // namespace gltfviewer

namespace gltfviewer
{
    class [[nodiscard]] shadow_map_t final
    {
    public:
        explicit shadow_map_t(vkrndr::backend_t& backend);

        shadow_map_t(shadow_map_t const&) = delete;

        shadow_map_t(shadow_map_t&&) noexcept = delete;

    public:
        ~shadow_map_t();

    public:
        [[nodiscard]] VkPipelineLayout pipeline_layout() const;

        [[nodiscard]] VkDescriptorSetLayout descriptor_layout() const;

        void load(VkDescriptorSetLayout environment_layout,
            VkDescriptorSetLayout materials_layout,
            VkFormat depth_buffer_format);

        void draw(scene_graph_t const& graph,
            VkCommandBuffer command_buffer) const;

        void bind_on(VkCommandBuffer command_buffer,
            VkPipelineLayout layout,
            VkPipelineBindPoint bind_point);

    public:
        shadow_map_t& operator=(shadow_map_t const&) = delete;

        shadow_map_t& operator=(shadow_map_t&&) noexcept = delete;

    private:
        vkrndr::backend_t* backend_;

        std::filesystem::file_time_type vertex_write_time_;
        vkrndr::shader_module_t vertex_shader_;

        vkrndr::image_t shadow_map_;

        VkSampler shadow_sampler_;
        VkDescriptorSetLayout descriptor_layout_;
        VkDescriptorSet descriptor_{VK_NULL_HANDLE};

        vkrndr::pipeline_layout_t depth_pipeline_layout_;
        vkrndr::pipeline_t depth_pipeline_;
    };
} // namespace gltfviewer
#endif
