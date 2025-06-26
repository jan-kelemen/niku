#ifndef RESHED_APPLICATION_INCLUDED
#define RESHED_APPLICATION_INCLUDED

#include <ngnwsi_application.hpp>
#include <ngnwsi_mouse.hpp>

#include <SDL3/SDL_events.h>

#include <memory>
#include <vector>

namespace ngntxt
{
    class freetype_context_t;
} // namespace ngntxt

namespace ngnwsi
{
    class imgui_layer_t;
} // namespace ngnwsi

namespace vkrndr
{
    class backend_t;
} // namespace vkrndr

namespace reshed
{
    class text_editor_t;
    class window_t;
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

        bool begin_frame() override;

        void draw() override;

        void debug_draw() override;

        void end_frame(bool has_rendered) override;

        void on_startup() override;

        void on_shutdown() override;

    private:
        std::vector<std::unique_ptr<window_t>> windows_;

        ngnwsi::mouse_t mouse_;

        std::shared_ptr<ngntxt::freetype_context_t> freetype_context_;

        std::unique_ptr<vkrndr::backend_t> backend_;
    };
} // namespace reshed
#endif
