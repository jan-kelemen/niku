#ifndef EDITOR_WINDOW_INCLUDED
#define EDITOR_WINDOW_INCLUDED

#include <ngntxt_font_face.hpp>

#include <vkrndr_instance.hpp>

#include <cstdint>
#include <memory>

union SDL_Event;

namespace ngnwsi
{
    class imgui_layer_t;
    class render_window_t;
    class sdl_text_input_guard_t;
} // namespace ngnwsi

namespace vkrndr
{
    class backend_t;
    struct rendering_context_t;
} // namespace vkrndr

namespace reshed
{
    class text_editor_t;
} // namespace reshed

namespace reshed
{
    class [[nodiscard]] editor_window_t final
    {
    public:
        editor_window_t(vkrndr::rendering_context_t context,
            uint32_t frames_in_flight);

        editor_window_t(std::unique_ptr<ngnwsi::render_window_t>&& window,
            vkrndr::rendering_context_t context,
            uint32_t frames_in_flight);

        editor_window_t(editor_window_t const&) = delete;

        editor_window_t(editor_window_t&&) noexcept;

    public:
        ~editor_window_t();

    public:
        [[nodiscard]] bool is_focused() const noexcept;

        [[nodiscard]] uint32_t window_id() const noexcept;

        void use_font(ngntxt::font_face_ptr_t font);

        [[nodiscard]] bool handle_event(SDL_Event const& event);

        void draw();

    public:
        editor_window_t& operator=(editor_window_t const&) = delete;

        editor_window_t& operator=(editor_window_t&&) noexcept;

    private:
        std::unique_ptr<vkrndr::backend_t> backend_;
        vkrndr::instance_ptr_t instance_;
        std::unique_ptr<ngnwsi::render_window_t> render_window_;
        std::unique_ptr<ngnwsi::imgui_layer_t> imgui_layer_;
        std::unique_ptr<ngnwsi::sdl_text_input_guard_t> text_input_guard_;

        std::unique_ptr<text_editor_t> editor_;
    };
} // namespace reshed

#endif
