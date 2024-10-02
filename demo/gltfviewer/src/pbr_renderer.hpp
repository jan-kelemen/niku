#ifndef GLTFVIEWER_PBR_RENDERER_INCLUDED
#define GLTFVIEWER_PBR_RENDERER_INCLUDED

#include <cppext_cycled_buffer.hpp>

#include <vkgltf_model.hpp>

#include <vkrndr_buffer.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_memory.hpp>
#include <vkrndr_pipeline.hpp>
#include <vkrndr_shader_module.hpp>

#include <volk.h>

#include <cstdint>
#include <filesystem>
#include <vector>

// IWYU pragma: no_include <chrono>

namespace vkrndr
{
    class backend_t;
} // namespace vkrndr

namespace gltfviewer
{
    class environment_t;
} // namespace gltfviewer

namespace gltfviewer
{
    class [[nodiscard]] pbr_renderer_t final
    {
    public:
        pbr_renderer_t(vkrndr::backend_t& backend, environment_t& environment);

        pbr_renderer_t(pbr_renderer_t const&) = delete;

        pbr_renderer_t(pbr_renderer_t&&) noexcept = delete;

    public:
        ~pbr_renderer_t();

    public:
        [[nodiscard]] VkPipelineLayout pipeline_layout() const;

        void load_model(vkgltf::model_t&& model);

        void draw(VkCommandBuffer command_buffer,
            vkrndr::image_t const& color_image);

        void resize(uint32_t width, uint32_t height);

    public:
        pbr_renderer_t& operator=(pbr_renderer_t const&) = delete;

        pbr_renderer_t& operator=(pbr_renderer_t&&) noexcept = delete;

    private:
        struct [[nodiscard]] frame_data_t final
        {
            vkrndr::buffer_t transform_uniform;
            vkrndr::mapped_memory_t transform_uniform_map;
            VkDescriptorSet transform_descriptor_set{VK_NULL_HANDLE};
        };

    private:
        void recreate_pipelines();

    private:
        vkrndr::backend_t* backend_;
        environment_t* environment_;

        vkrndr::image_t depth_buffer_;

        std::filesystem::file_time_type vertex_write_time_;
        vkrndr::shader_module_t vertex_shader_;

        std::filesystem::file_time_type fragment_write_time_;
        vkrndr::shader_module_t fragment_shader_;

        vkgltf::model_t model_;

        std::vector<VkSampler> samplers_;
        vkrndr::buffer_t material_uniform_;
        VkDescriptorSet material_descriptor_set_{VK_NULL_HANDLE};

        VkDescriptorSetLayout material_descriptor_set_layout_{VK_NULL_HANDLE};
        VkDescriptorSetLayout transform_descriptor_set_layout_{VK_NULL_HANDLE};
        vkrndr::pipeline_t double_sided_pipeline_;
        vkrndr::pipeline_t culling_pipeline_;
        vkrndr::pipeline_t blending_pipeline_;

        cppext::cycled_buffer_t<frame_data_t> frame_data_;

        uint32_t debug_{0};
    };
} // namespace gltfviewer
#endif
