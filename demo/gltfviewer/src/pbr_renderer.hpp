#ifndef GLTFVIEWER_PBR_RENDERER_INCLUDED
#define GLTFVIEWER_PBR_RENDERER_INCLUDED

#include <cppext_cycled_buffer.hpp>

#include <vkgltf_model.hpp>

#include <vkrndr_buffer.hpp>
#include <vkrndr_memory.hpp>
#include <vkrndr_pipeline.hpp>

#include <vulkan/vulkan_core.h>

#include <cstdint>

namespace niku
{
    class camera_t;
} // namespace niku

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
        void load_model(vkgltf::model_t&& model);

        void update(niku::camera_t const& camera);

        void draw(VkCommandBuffer command_buffer,
            vkrndr::image_t const& target_image);

    public:
        pbr_renderer_t& operator=(pbr_renderer_t const&) = delete;

        pbr_renderer_t& operator=(pbr_renderer_t&&) noexcept = delete;

    private:
        struct [[nodiscard]] frame_data_t final
        {
            vkrndr::buffer_t camera_uniform;
            vkrndr::mapped_memory_t camera_uniform_map;
            VkDescriptorSet camera_descriptor_set{VK_NULL_HANDLE};
        };

    private:
        vkrndr::backend_t* backend_;

        vkgltf::model_t model_;

        VkDescriptorSetLayout descriptor_set_layout_{VK_NULL_HANDLE};
        vkrndr::pipeline_t pipeline_;

        cppext::cycled_buffer_t<frame_data_t> frame_data_;
    };
} // namespace gltfviewer
#endif
