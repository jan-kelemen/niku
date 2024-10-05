#ifndef NIKU_APPLICATION_INCLUDED
#define NIKU_APPLICATION_INCLUDED

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_video.h>

#include <cstdint>
#include <memory>
#include <string_view>

namespace niku
{
    class sdl_window_t;
} // namespace niku

namespace niku
{
    struct [[nodiscard]] subsystems_t final
    {
        bool video{true};
        bool debug{false};
    };

    struct [[nodiscard]] startup_params_t final
    {
        subsystems_t init_subsystems;

        std::string_view title;

        SDL_WindowFlags window_flags;
        bool centered{true};
        int width;
        int height;
    };

    class [[nodiscard]] application_t
    {
    public:
        explicit application_t(startup_params_t const& params);

        application_t(application_t const&) = delete;

        application_t(application_t&&) noexcept = default;

    public:
        virtual ~application_t();

    public:
        void run();

        void fixed_update_interval(float delta_time);

        [[nodiscard]] float fixed_update_interval() const;

        [[nodiscard]] sdl_window_t* window();

    private: // Callback interface
        [[nodiscard]] virtual bool should_run() { return true; }

        [[nodiscard]] virtual bool is_quit_event(SDL_Event const& event);

        virtual bool handle_event([[maybe_unused]] SDL_Event const& event)
        {
            return true;
        }

        virtual void fixed_update([[maybe_unused]] float const delta_time) { }

        virtual void update([[maybe_unused]] float const delta_time) { }

        [[nodiscard]] virtual bool begin_frame() { return false; }

        virtual void draw() { }

        virtual void end_frame() { }

        virtual void on_startup() { }

        virtual void on_shutdown() { }

        virtual void on_resize([[maybe_unused]] uint32_t width,
            [[maybe_unused]] uint32_t height)
        {
        }

    public:
        application_t& operator=(application_t const&) = delete;

        application_t& operator=(application_t&&) noexcept = delete;

    private:
        struct impl;
        std::unique_ptr<impl> impl_;
    };
} // namespace niku

#endif
