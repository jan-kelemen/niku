#ifndef RESHED_WINDOW_INCLUDED
#define RESHED_WINDOW_INCLUDED

#include <ngntxt_font_face.hpp>

#include <ngnwsi_imgui_layer.hpp>
#include <ngnwsi_window.hpp>

union SDL_Event;

namespace vkrndr
{
    class backend_t;
} // namespace vkrndr

namespace reshed
{
    class text_editor_t;
} // namespace reshed

namespace reshed
{
    class [[nodiscard]] window_t final : public ngnwsi::window_t
    {
    public:
        window_t();

        window_t(window_t const&) = delete;

        window_t(window_t&&) noexcept = delete;

    public:
        ~window_t() override = default;

    public:
        void init_rendering(vkrndr::backend_t& backend);

        void set_font(ngntxt::font_face_ptr_t font_face);

        void handle_event(SDL_Event const& event) override;

        bool begin_frame();

        void render(vkrndr::backend_t& backend);

        void debug_draw();

        void end_frame();

    public:
        window_t& operator=(window_t const&) = delete;

        window_t& operator=(window_t&&) noexcept = delete;

    private:
        ngnwsi::sdl_text_input_guard_t text_input_;
        std::unique_ptr<ngnwsi::imgui_layer_t> imgui_;
        std::unique_ptr<text_editor_t> editor_;
    };
} // namespace reshed
#endif
