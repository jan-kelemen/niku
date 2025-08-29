#ifndef GALILEO_POSTPROCESS_SHADER_INCLUDED
#define GALILEO_POSTPROCESS_SHADER_INCLUDED

#include <cppext_cycled_buffer.hpp>

#include <vkrndr_image.hpp>
#include <vkrndr_pipeline.hpp>

#include <volk.h>

#include <cstdint>
#include <functional>

namespace vkrndr
{
    class backend_t;
} // namespace vkrndr

namespace galileo
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
        void draw(VkCommandBuffer command_buffer,
            vkrndr::image_t const& color_image,
            vkrndr::image_t const& target_image);

        void resize(uint32_t width,
            uint32_t height,
            std::function<void(std::function<void()>)>& deletion_queue_insert);

    public:
        postprocess_shader_t& operator=(postprocess_shader_t const&) = delete;

        postprocess_shader_t& operator=(postprocess_shader_t&&) = delete;

    private:
        struct [[nodiscard]] frame_data_t final
        {
            VkDescriptorSet tone_mapping_descriptor_set;
            VkDescriptorSet fxaa_descriptor_set;
        };

    private:
        vkrndr::backend_t* backend_;

        VkSampler sampler_;

        vkrndr::image_t intermediate_image_;

        VkDescriptorSetLayout tone_mapping_descriptor_set_layout_{
            VK_NULL_HANDLE};
        VkDescriptorSetLayout fxaa_descriptor_set_layout_{VK_NULL_HANDLE};

        vkrndr::pipeline_t tone_mapping_pipeline_;
        vkrndr::pipeline_t fxaa_pipeline_;

        cppext::cycled_buffer_t<frame_data_t> frame_data_;
    };
} // namespace galileo

#endif
