#ifndef GALILEO_DEFERRED_SHADER_INCLUDED
#define GALILEO_DEFERRED_SHADER_INCLUDED

#include <cppext_cycled_buffer.hpp>

#include <vkrndr_pipeline.hpp>

#include <volk.h>

namespace vkrndr
{
    class backend_t;
} // namespace vkrndr

namespace galileo
{
    class gbuffer_t;
} // namespace galileo

namespace galileo
{
    class [[nodiscard]] deferred_shader_t final
    {
    public:
        explicit deferred_shader_t(vkrndr::backend_t& backend,
            VkDescriptorSetLayout frame_info_layout);

        deferred_shader_t(deferred_shader_t const&) = delete;

        deferred_shader_t(deferred_shader_t&&) noexcept = delete;

    public:
        ~deferred_shader_t();

    public:
        [[nodiscard]] VkPipelineLayout pipeline_layout() const;

        void draw(VkCommandBuffer command_buffer, gbuffer_t& gbuffer);

    public:
        deferred_shader_t& operator=(deferred_shader_t const&) = delete;

        deferred_shader_t& operator=(deferred_shader_t&&) noexcept = delete;

    private:
        struct [[nodiscard]] frame_data_t final
        {
            VkDescriptorSet descriptor_set;
        };

    private:
        vkrndr::backend_t* backend_;

        VkDescriptorSetLayout descriptor_set_layout_;
        VkSampler sampler_;

        vkrndr::pipeline_t pipeline_;

        cppext::cycled_buffer_t<frame_data_t> frame_data_;
    };
} // namespace galileo
#endif
