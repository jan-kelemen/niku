#ifndef GLTFVIEWER_CAMERA_CONTROLLER_INCLUDED
#define GLTFVIEWER_CAMERA_CONTROLLER_INCLUDED

#include <glm/vec3.hpp>

union SDL_Event;

namespace niku
{
    class mouse_t;
    class perspective_camera_t;
} // namespace niku

namespace gltfviewer
{
    class [[nodiscard]] camera_controller_t final
    {
    public:
        camera_controller_t(niku::perspective_camera_t& camera,
            niku::mouse_t& mouse);

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
        niku::mouse_t* mouse_;
        niku::perspective_camera_t* camera_;

        glm::vec3 velocity_{};
        bool update_needed_{false};

        float mouse_sensitivity_{0.1f};
        float velocity_factor_{5.0f};
    };
} // namespace gltfviewer

#endif
