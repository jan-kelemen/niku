#ifndef GLTFVIEWER_RESOLVE_SHADER_INCLUDED
#define GLTFVIEWER_RESOLVE_SHADER_INCLUDED

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
    class [[nodiscard]] resolve_shader_t final
    {
    public:
        explicit resolve_shader_t(vkrndr::backend_t& backend);

        resolve_shader_t(resolve_shader_t const&) = delete;

        resolve_shader_t(resolve_shader_t&&) noexcept = delete;

    public:
        ~resolve_shader_t();

    public:
        void draw(VkCommandBuffer command_buffer,
            vkrndr::image_t const& color_image,
            vkrndr::image_t const& target_image,
            vkrndr::image_t const& bright_image);

    public:
        resolve_shader_t& operator=(resolve_shader_t const&) = delete;

        resolve_shader_t& operator=(resolve_shader_t&&) = delete;

    private:
        vkrndr::backend_t* backend_;

        VkSampler combined_sampler_;

        VkDescriptorSetLayout descriptor_set_layout_{VK_NULL_HANDLE};
        cppext::cycled_buffer_t<VkDescriptorSet> descriptor_sets_;

        vkrndr::pipeline_t pipeline_;
    };
} // namespace gltfviewer

#endif
