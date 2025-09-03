#ifndef RESHED_APPLICATION_INCLUDED
#define RESHED_APPLICATION_INCLUDED

#include <editor_window.hpp> // IWYU pragma: keep

#include <ngnwsi_application.hpp>

#include <vkrndr_backend.hpp>

#include <SDL3/SDL_events.h>

#include <memory>
#include <vector>

namespace ngntxt
{
    class freetype_context_t;
} // namespace ngntxt

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

    private:
        vkrndr::rendering_context_t rendering_context_;

        std::shared_ptr<ngntxt::freetype_context_t> freetype_context_;
        std::vector<editor_window_t> windows_;
    };
} // namespace reshed
#endif
