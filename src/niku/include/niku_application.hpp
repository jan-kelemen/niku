#ifndef NIKU_APPLICATION_INCLUDED
#define NIKU_APPLICATION_INCLUDED

#include <vkrndr_render_settings.hpp>

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_video.h>

#include <memory>
#include <string_view>

namespace vkrndr
{
    struct vulkan_device;
    class vulkan_renderer;
    class scene;
} // namespace vkrndr

namespace niku
{
    struct [[nodiscard]] subsystems final
    {
        bool video{true};
        bool audio{false};
        bool debug{false};
    };

    struct [[nodiscard]] startup_params final
    {
        subsystems init_subsystems;

        std::string_view title;

        SDL_WindowFlags window_flags;
        bool centered{true};
        int width;
        int height;

        vkrndr::render_settings render;
    };

    class [[nodiscard]] application
    {
    public:
        explicit application(startup_params const& params);

        application(application const&) = delete;

        application(application&&) noexcept = default;

    public:
        virtual ~application();

    public:
        void run();

        void fixed_update_interval(float delta_time);

        [[nodiscard]] float fixed_update_interval() const;

        void debug_layer(bool enable);

        [[nodiscard]] bool debug_layer() const;

        [[nodiscard]] vkrndr::vulkan_device* vulkan_device();

        [[nodiscard]] vkrndr::vulkan_renderer* vulkan_renderer();

    private: // Callback interface
        [[nodiscard]] virtual bool should_run() { return true; }

        [[nodiscard]] virtual bool is_quit_event(SDL_Event const& event);

        virtual bool handle_event([[maybe_unused]] SDL_Event const& event)
        {
            return true;
        }

        virtual void begin_frame() { }

        virtual void fixed_update([[maybe_unused]] float const delta_time) { }

        virtual void update([[maybe_unused]] float const delta_time) { }

        [[nodiscard]] virtual vkrndr::scene* render_scene() = 0;

        virtual void end_frame() { }

        virtual void on_startup() { }

        virtual void on_shutdown() { }

    public:
        application& operator=(application const&) = delete;

        application& operator=(application&&) noexcept = delete;

    private:
        struct impl;
        std::unique_ptr<impl> impl_;
    };
} // namespace niku

#endif
