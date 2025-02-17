#ifndef NGNGFX_PERSPECTIVE_CAMERA_INCLUDED
#define NGNGFX_PERSPECTIVE_CAMERA_INCLUDED

#include <ngngfx_camera.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace ngngfx
{
    class [[nodiscard]] perspective_camera_t : public camera_t
    {
    public:
        perspective_camera_t();

        perspective_camera_t(glm::vec3 const& position,
            float aspect_ratio,
            float fov,
            glm::vec3 const& world_up,
            glm::vec2 const& near_far_planes,
            glm::vec2 const& yaw_pitch);

        perspective_camera_t(perspective_camera_t const&) = default;

        perspective_camera_t(perspective_camera_t&&) noexcept = default;

    public:
        ~perspective_camera_t() override = default;

    public:
        [[nodiscard]] glm::vec2 yaw_pitch() const;

        void set_yaw_pitch(glm::vec2 const& yaw_pitch);

        void set_near_far(glm::vec2 const& near_far);

        [[nodiscard]] glm::vec3 up_direction() const;

        [[nodiscard]] glm::vec3 front_direction() const;

        [[nodiscard]] glm::vec3 right_direction() const;

    public: // camera_t overrides
        void update() override;

        [[nodiscard]] glm::mat4 const& view_matrix() const override;

        [[nodiscard]] glm::mat4 const& projection_matrix() const override;

        [[nodiscard]] glm::mat4 const& view_projection_matrix() const override;

    public:
        perspective_camera_t& operator=(perspective_camera_t const&) = default;

        perspective_camera_t& operator=(
            perspective_camera_t&&) noexcept = default;

    private:
        void calculate_view_projection_matrices();

    protected:
        glm::vec3 world_up_;
        glm::vec2 near_far_planes_;
        glm::vec2 yaw_pitch_;
        float fov_;

        glm::vec3 up_direction_;
        glm::vec3 front_direction_;
        glm::vec3 right_direction_;

        glm::mat4 view_matrix_;
        glm::mat4 projection_matrix_;
        glm::mat4 view_projection_matrix_;
    };
} // namespace ngngfx

#endif
