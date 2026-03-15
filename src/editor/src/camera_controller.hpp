#ifndef EDITOR_CAMERA_CONTROLLER_INCLUDED
#define EDITOR_CAMERA_CONTROLLER_INCLUDED

#include <glm/vec3.hpp>

union SDL_Event;

namespace ngngfx
{
    class aircraft_camera_t;
} // namespace ngngfx

namespace ngnwsi
{
    class mouse_t;
} // namespace ngnwsi

namespace editor
{
    class [[nodiscard]] camera_controller_t final
    {
    public:
        camera_controller_t(ngngfx::aircraft_camera_t& camera,
            ngnwsi::mouse_t& mouse);

        camera_controller_t(camera_controller_t const&) = delete;

        camera_controller_t(camera_controller_t&&) noexcept = delete;

    public:
        ~camera_controller_t() = default;

    public:
        void handle_event(SDL_Event const& event);

        bool update(float delta_time);

        void draw_imgui();

    public:
        camera_controller_t& operator=(camera_controller_t const&) = delete;

        camera_controller_t& operator=(camera_controller_t&&) noexcept = delete;

    private:
        ngngfx::aircraft_camera_t* camera_;
        ngnwsi::mouse_t* mouse_;

        glm::vec3 velocity_{};
        bool update_needed_{false};

        float mouse_sensitivity_{0.2f};
        float velocity_factor_{1.0f};
    };
} // namespace editor

#endif
