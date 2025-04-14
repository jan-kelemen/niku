#ifndef NGNGFX_AIRCRAFT_CAMERA_INCLUDED
#define NGNGFX_AIRCRAFT_CAMERA_INCLUDED

#include <ngngfx_camera.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace ngngfx
{
    class [[nodiscard]] aircraft_camera_t : public camera_t
    {
    public:
        aircraft_camera_t();

        aircraft_camera_t(glm::vec3 const& position,
            glm::vec3 const& world_up,
            glm::vec2 const& yaw_pitch);

        aircraft_camera_t(aircraft_camera_t const&) = default;

        aircraft_camera_t(aircraft_camera_t&&) noexcept = default;

    public:
        ~aircraft_camera_t() override = default;

    public:
        [[nodiscard]] glm::vec2 yaw_pitch() const;

        void set_yaw_pitch(glm::vec2 const& yaw_pitch);

        [[nodiscard]] glm::vec3 up_direction() const;

        [[nodiscard]] glm::vec3 front_direction() const;

        [[nodiscard]] glm::vec3 right_direction() const;

    public: // camera_t overrides
        void update() override;

        [[nodiscard]] glm::mat4 const& view_matrix() const override;

    public:
        aircraft_camera_t& operator=(aircraft_camera_t const&) = default;

        aircraft_camera_t& operator=(aircraft_camera_t&&) noexcept = default;

    private:
        void calculate_view_matrix();

    protected:
        glm::vec3 world_up_;
        glm::vec2 yaw_pitch_;

        glm::vec3 up_direction_;
        glm::vec3 front_direction_;
        glm::vec3 right_direction_;

        glm::mat4 view_matrix_;
    };
} // namespace ngngfx

#endif
