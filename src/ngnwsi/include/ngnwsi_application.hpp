#ifndef NGNWSI_APPLICATION_INCLUDED
#define NGNWSI_APPLICATION_INCLUDED

#include <SDL3/SDL_events.h>

#include <memory>

namespace ngnwsi
{
    struct [[nodiscard]] subsystems_t final
    {
        bool video{true};
        bool debug{false};
    };

    struct [[nodiscard]] startup_params_t final
    {
        subsystems_t init_subsystems;
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

        void fixed_update_interval(float fps);

    private: // Callback interface
        [[nodiscard]] virtual bool should_run() { return true; }

        virtual bool handle_event([[maybe_unused]] SDL_Event const& event)
        {
            return true;
        }

        virtual void update([[maybe_unused]] float const delta_time) { }

        virtual void begin_frame() { }

        virtual void draw() { }

        virtual void end_frame() { }

        virtual void on_startup() { }

        virtual void on_shutdown() { }

    public:
        application_t& operator=(application_t const&) = delete;

        application_t& operator=(application_t&&) noexcept = delete;

    private:
        struct impl;
        std::unique_ptr<impl> impl_;
    };
} // namespace ngnwsi

#endif
