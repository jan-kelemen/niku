#ifndef RESHED_APPLICATION_INCLUDED
#define RESHED_APPLICATION_INCLUDED

#include <ngnwsi_application.hpp>
#include <ngnwsi_mouse.hpp>
#include <ngnwsi_render_window.hpp>

#include <SDL3/SDL_events.h>

#include <memory>

namespace ngntxt
{
    class freetype_context_t;
} // namespace ngntxt

namespace ngnwsi
{
    class imgui_layer_t;
    class sdl_text_input_guard_t;
} // namespace ngnwsi

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
    class [[nodiscard]] application_t final : public ngnwsi::application_t
    {
    public:
        explicit application_t(bool debug);

        application_t(application_t const&) = delete;

        application_t(application_t&&) noexcept = delete;

    public:
        ~application_t() override;

    public:
        // cppcheck-suppress duplInheritedMember
        application_t& operator=(application_t const&) = delete;

        // cppcheck-suppress duplInheritedMember
        application_t& operator=(application_t&&) noexcept = delete;

    private: // ngnwsi::application callback interface
        bool should_run() override;

        bool handle_event(SDL_Event const& event) override;

        void draw() override;

        void end_frame() override;

        void on_startup() override;

        void on_shutdown() override;

        void on_resize(uint32_t width, uint32_t height) override;

    private:
        ngnwsi::mouse_t mouse_;

        std::shared_ptr<ngntxt::freetype_context_t> freetype_context_;
        std::unique_ptr<ngnwsi::render_window_t> render_window_;
        std::unique_ptr<vkrndr::backend_t> backend_;
        std::unique_ptr<ngnwsi::imgui_layer_t> imgui_;

        std::unique_ptr<text_editor_t> editor_;

        std::unique_ptr<ngnwsi::sdl_text_input_guard_t> text_input_guard_;
    };
} // namespace reshed
#endif
