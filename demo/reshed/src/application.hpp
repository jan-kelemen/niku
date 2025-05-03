#ifndef RESHED_APPLICATION_INCLUDED
#define RESHED_APPLICATION_INCLUDED

#include <ngntxt_font_bitmap.hpp>

#include <ngnwsi_application.hpp>
#include <ngnwsi_mouse.hpp>

#include <SDL3/SDL_events.h>

#include <memory>

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
        bool handle_event(SDL_Event const& event, float delta_time) override;

        bool begin_frame() override;

        void draw() override;

        void end_frame() override;

        void on_startup() override;

        void on_shutdown() override;

    private:
        ngnwsi::mouse_t mouse_;

        std::unique_ptr<vkrndr::backend_t> backend_;
        std::unique_ptr<ngnwsi::imgui_layer_t> imgui_;

        ngntxt::font_bitmap_t font_bitmap_;
    };
} // namespace reshed
#endif
