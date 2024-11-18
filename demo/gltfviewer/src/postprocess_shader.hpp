#ifndef GLTFVIEWER_POSTPROCESS_SHADER_INCLUDED
#define GLTFVIEWER_POSTPROCESS_SHADER_INCLUDED

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
    class [[nodiscard]] postprocess_shader_t final
    {
    public:
        explicit postprocess_shader_t(vkrndr::backend_t& backend);

        postprocess_shader_t(postprocess_shader_t const&) = delete;

        postprocess_shader_t(postprocess_shader_t&&) noexcept = delete;

    public:
        ~postprocess_shader_t();

    public:
        void draw(bool color_conversion,
            bool tone_mapping,
            VkCommandBuffer command_buffer,
            vkrndr::image_t const& color_image,
            vkrndr::image_t const& target_image);

    public:
        postprocess_shader_t& operator=(postprocess_shader_t const&) = delete;

        postprocess_shader_t& operator=(postprocess_shader_t&&) = delete;

    private:
        vkrndr::backend_t* backend_;

        VkDescriptorSetLayout descriptor_set_layout_{VK_NULL_HANDLE};
        cppext::cycled_buffer_t<VkDescriptorSet> descriptor_sets_;

        vkrndr::pipeline_t pipeline_;
    };
} // namespace gltfviewer

#endif
