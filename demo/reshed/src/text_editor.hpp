#ifndef RESHED_TEXT_EDITOR_INCLUDED
#define RESHED_TEXT_EDITOR_INCLUDED

#include <cppext_cycled_buffer.hpp>

#include <ngngfx_orthographic_projection.hpp>

#include <ngntxt_font_bitmap.hpp>
#include <ngntxt_font_face.hpp>
#include <ngntxt_shaping.hpp>

#include <vkrndr_buffer.hpp>
#include <vkrndr_memory.hpp>
#include <vkrndr_pipeline.hpp>

#include <volk.h>

namespace vkrndr
{
    class backend_t;
} // namespace vkrndr

namespace reshed
{
    class [[nodiscard]] text_editor_t final
    {
    public:
        text_editor_t(vkrndr::backend_t& backend);

        text_editor_t(text_editor_t const&) = delete;

        text_editor_t(text_editor_t&&) noexcept = delete;

    public:
        ~text_editor_t();

    public:
        void change_font(ngntxt::font_face_ptr_t font_face);

        [[nodiscard]] VkPipelineLayout pipeline_layout() const;

        void draw(VkCommandBuffer command_buffer);

    public:
        text_editor_t& operator=(text_editor_t const&) = delete;

        text_editor_t& operator=(text_editor_t&&) noexcept = delete;

    private:
        struct [[nodiscard]] frame_data_t final
        {
            vkrndr::buffer_t vertex_buffer;
            vkrndr::mapped_memory_t vertex_map;
        };

    private:
        vkrndr::backend_t* backend_;

        ngngfx::orthographic_projection_t projection_;

        bool content_changed_{true};

        ngntxt::font_face_ptr_t font_face_;
        ngntxt::font_bitmap_t font_bitmap_;

        ngntxt::shaping_font_face_ptr_t shaping_font_face_;
        ngntxt::shaping_buffer_ptr_t shaping_buffer_;

        VkSampler bitmap_sampler_;

        VkDescriptorSetLayout text_descriptor_layout_;
        VkDescriptorSet text_descriptor_;

        vkrndr::pipeline_t text_pipeline_;

        cppext::cycled_buffer_t<frame_data_t> frame_data_;
    };
} // namespace reshed

#endif
