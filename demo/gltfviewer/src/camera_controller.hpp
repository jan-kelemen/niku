#ifndef GLTFVIEWER_CAMERA_CONTROLLER_INCLUDED
#define GLTFVIEWER_CAMERA_CONTROLLER_INCLUDED

#include <glm/vec3.hpp>

union SDL_Event;

namespace ngngfx
{
    class perspective_camera_t;
} // namespace ngngfx

namespace ngnwsi
{
    class mouse_t;
} // namespace ngnwsi

namespace gltfviewer
{
    class [[nodiscard]] camera_controller_t final
    {
    public:
        camera_controller_t(ngngfx::perspective_camera_t& camera,
            ngnwsi::mouse_t& mouse);

        camera_controller_t(camera_controller_t const&) = delete;

        camera_controller_t(camera_controller_t&&) noexcept = delete;

    public:
        ~camera_controller_t() = default;

    public:
        void handle_event(SDL_Event const& event, float delta_time);

        bool update(float delta_time);

        void draw_imgui();

    public:
        camera_controller_t& operator=(camera_controller_t const&) = delete;

        camera_controller_t& operator=(camera_controller_t&&) noexcept = delete;

    private:
        ngngfx::perspective_camera_t* camera_;
        ngnwsi::mouse_t* mouse_;

        glm::vec3 velocity_{};
        bool update_needed_{false};

        float mouse_sensitivity_{5.0f};
        float velocity_factor_{5.0f};
    };
} // namespace gltfviewer

#endif
