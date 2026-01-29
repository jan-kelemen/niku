#ifndef EDITOR_APPLICATION_INCLUDED
#define EDITOR_APPLICATION_INCLUDED

#include <vkrndr_rendering_context.hpp>

#include <memory>
#include <span>

union SDL_Event;

namespace ngnwsi
{
    class render_window_t;
} // namespace ngnwsi

namespace editor
{
    class [[nodiscard]] application_t final
    {
    public:
        explicit application_t(std::span<char const*> command_line_parameters);

        application_t(application_t const&) = default;

        application_t(application_t&&) noexcept = default;

    public:
        ~application_t();

    public:
        [[nodiscard]] bool handle_event(SDL_Event const& event);

        [[nodiscard]] bool update();

    public:
        application_t& operator=(application_t const&) = default;

        application_t& operator=(application_t&&) noexcept = default;

    private:
        application_t();

    private:
        void process_command_line(std::span<char const*> const& parameters);

    private:
        std::unique_ptr<ngnwsi::render_window_t> main_window_;
        vkrndr::rendering_context_t rendering_context_;
    };
} // namespace editor

#endif // !EDITOR_APPLICATION_INCLUDED
