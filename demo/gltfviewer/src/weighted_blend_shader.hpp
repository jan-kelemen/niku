#ifndef GLTFVIEWER_WEIGHTED_BLEND_INCLUDED
#define GLTFVIEWER_WEIGHTED_BLEND_INCLUDED

#include <cppext_cycled_buffer.hpp>

#include <vkrndr_pipeline.hpp>

#include <volk.h>

namespace vkrndr
{
    class backend_t;
    struct image_t;
} // namespace vkrndr

namespace gltfviewer
{
    class [[nodiscard]] weighted_blend_shader_t final
    {
    public:
        explicit weighted_blend_shader_t(vkrndr::backend_t& backend);

        weighted_blend_shader_t(weighted_blend_shader_t const&) = delete;

        weighted_blend_shader_t(weighted_blend_shader_t&&) noexcept = delete;

    public:
        ~weighted_blend_shader_t();

    public:
        void draw(float bias,
            VkCommandBuffer command_buffer,
            vkrndr::image_t const& target_image,
            vkrndr::image_t const& other_image);

    public:
        weighted_blend_shader_t& operator=(
            weighted_blend_shader_t const&) = delete;

        weighted_blend_shader_t& operator=(
            weighted_blend_shader_t&&) noexcept = delete;

    private:
        struct [[nodiscard]] frame_data_t final
        {
            VkDescriptorSet descriptor_{VK_NULL_HANDLE};
        };

    private:
        vkrndr::backend_t* backend_;

        VkSampler bilinear_sampler_;

        VkDescriptorSetLayout descriptor_layout_{VK_NULL_HANDLE};
        vkrndr::pipeline_t pipeline_;

        cppext::cycled_buffer_t<frame_data_t> frame_data_;
    };
} // namespace gltfviewer

#endif
