#ifndef GLTFVIEWER_PBR_RENDERER_INCLUDED
#define GLTFVIEWER_PBR_RENDERER_INCLUDED

#include <cppext_cycled_buffer.hpp>

#include <vkgltf_model.hpp>

#include <vkrndr_buffer.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_memory.hpp>
#include <vkrndr_pipeline.hpp>

#include <volk.h>

#include <cstdint>
#include <vector>

namespace niku
{
    class camera_t;
} // namespace niku

namespace vkrndr
{
    class backend_t;
} // namespace vkrndr

namespace gltfviewer
{
    class [[nodiscard]] pbr_renderer_t final
    {
    public:
        explicit pbr_renderer_t(vkrndr::backend_t* backend);

        pbr_renderer_t(pbr_renderer_t const&) = delete;

        pbr_renderer_t(pbr_renderer_t&&) noexcept = delete;

    public:
        ~pbr_renderer_t();

    public:
        void load_model(vkgltf::model_t&& model);

        void update(niku::camera_t const& camera);

        void draw(VkCommandBuffer command_buffer,
            vkrndr::image_t const& target_image);

        void resize(uint32_t width, uint32_t height);

    public:
        pbr_renderer_t& operator=(pbr_renderer_t const&) = delete;

        pbr_renderer_t& operator=(pbr_renderer_t&&) noexcept = delete;

    private:
        struct [[nodiscard]] frame_data_t final
        {
            vkrndr::buffer_t camera_uniform;
            vkrndr::mapped_memory_t camera_uniform_map;
            VkDescriptorSet camera_descriptor_set{VK_NULL_HANDLE};

            vkrndr::buffer_t transform_uniform;
            vkrndr::mapped_memory_t transform_uniform_map;
            VkDescriptorSet transform_descriptor_set{VK_NULL_HANDLE};
        };

    private:
        void recreate_pipeline();

    private:
        vkrndr::backend_t* backend_;
        vkrndr::image_t depth_buffer_;

        vkgltf::model_t model_;

        std::vector<VkSampler> samplers_;
        vkrndr::buffer_t material_uniform_;
        VkDescriptorSet material_descriptor_set_{VK_NULL_HANDLE};

        VkDescriptorSetLayout camera_descriptor_set_layout_{VK_NULL_HANDLE};
        VkDescriptorSetLayout material_descriptor_set_layout_{VK_NULL_HANDLE};
        VkDescriptorSetLayout transform_descriptor_set_layout_{VK_NULL_HANDLE};
        vkrndr::pipeline_t pipeline_;

        cppext::cycled_buffer_t<frame_data_t> frame_data_;
    };
} // namespace gltfviewer
#endif
