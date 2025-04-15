#include <ngngfx_perspective_projection.hpp>

#include <ngngfx_camera.hpp>
#include <ngngfx_projection.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/trigonometric.hpp>

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
ngngfx::perspective_projection_t::perspective_projection_t(
    camera_t const& camera,
    glm::vec2 const near_far_planes,
    float const aspect_ratio,
    float const fov)
    : projection_t{camera, near_far_planes, aspect_ratio}
    , fov_{fov}
{
    calculate_view_projection_matrices();
}

void ngngfx::perspective_projection_t::update()
{
    calculate_view_projection_matrices();
}

glm::mat4 const& ngngfx::perspective_projection_t::projection_matrix() const
{
    return projection_matrix_;
}

glm::mat4 const&
ngngfx::perspective_projection_t::view_projection_matrix() const
{
    return view_projection_matrix_;
}

void ngngfx::perspective_projection_t::calculate_view_projection_matrices()
{
    projection_matrix_ = glm::perspectiveRH_ZO(glm::radians(fov_),
        aspect_ratio_,
        near_far_planes_.x,
        near_far_planes_.y);

    projection_matrix_[1][1] *= -1;

    view_projection_matrix_ = camera_->view_matrix() * projection_matrix_;
}
