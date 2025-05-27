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

#include <cstdint>
#include <string>

union SDL_Event;

namespace vkrndr
{
    class backend_t;
} // namespace vkrndr

namespace reshed
{
    class [[nodiscard]] text_buffer_t final
    {
    public:
        void add(size_t line, size_t column, std::string_view content);

        void remove(size_t line, size_t column, size_t count);

        [[nodiscard]] std::string_view line(size_t line) const;

        [[nodiscard]] size_t lines() const;

    private:
        std::string buffer_;
    };

    class [[nodiscard]] text_editor_t final
    {
    public:
        explicit text_editor_t(vkrndr::backend_t& backend);

        text_editor_t(text_editor_t const&) = delete;

        text_editor_t(text_editor_t&&) noexcept = delete;

    public:
        ~text_editor_t();

    public:
        void handle_event(SDL_Event const& event);

        void change_font(ngntxt::font_face_ptr_t font_face);

        [[nodiscard]] VkPipelineLayout pipeline_layout() const;

        void draw(VkCommandBuffer command_buffer);

        void resize(uint32_t width, uint32_t height);

    public:
        text_editor_t& operator=(text_editor_t const&) = delete;

        text_editor_t& operator=(text_editor_t&&) noexcept = delete;

    private:
        struct [[nodiscard]] frame_data_t final
        {
            vkrndr::buffer_t vertex_buffer;
            vkrndr::mapped_memory_t vertex_map;
            uint32_t vertices{};

            vkrndr::buffer_t index_buffer;
            vkrndr::mapped_memory_t index_map;
            uint32_t indices{};
        };

    private:
        vkrndr::backend_t* backend_;

        ngngfx::orthographic_projection_t projection_;

        text_buffer_t buffer_;
        size_t cursor_line{};
        size_t cursor_column{};

        ngntxt::font_face_ptr_t font_face_;
        ngntxt::font_bitmap_t font_bitmap_;

        ngntxt::shaping_font_face_ptr_t shaping_font_face_;
        ngntxt::shaping_buffer_ptr_t shaping_buffer_;

        VkSampler bitmap_sampler_;

        VkDescriptorSetLayout text_descriptor_layout_;
        VkDescriptorSet text_descriptor_{VK_NULL_HANDLE};

        vkrndr::pipeline_t text_pipeline_;

        cppext::cycled_buffer_t<frame_data_t> frame_data_;
    };
} // namespace reshed

#endif
