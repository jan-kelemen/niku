#ifndef NGNGFX_FIXED_POINT_CAMERA_INCLUDED
#define NGNGFX_FIXED_POINT_CAMERA_INCLUDED

#include <ngngfx_camera.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace ngngfx
{
    class [[nodiscard]] fixed_point_camera_t : public camera_t
    {
    public:
        fixed_point_camera_t();

        fixed_point_camera_t(glm::vec3 const& position,
            glm::vec3 const& target,
            glm::vec3 const& world_up);

        fixed_point_camera_t(fixed_point_camera_t const&) = default;

        fixed_point_camera_t(fixed_point_camera_t&&) noexcept = default;

    public:
        ~fixed_point_camera_t() override = default;

    public:
        void set_target(glm::vec3 const& target);

        [[nodiscard]] glm::vec3 const& target() const;

    public: // camera_t overrides
        void update() override;

        [[nodiscard]] glm::mat4 const& view_matrix() const override;

    public:
        fixed_point_camera_t& operator=(fixed_point_camera_t const&) = default;

        fixed_point_camera_t& operator=(
            fixed_point_camera_t&&) noexcept = default;

    private:
        void calculate_view_matrix();

    protected:
        glm::vec3 world_up_;
        glm::vec3 target_;

        glm::mat4 view_matrix_;
    };
} // namespace ngngfx

#endif
