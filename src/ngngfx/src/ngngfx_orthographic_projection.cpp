#include <ngngfx_orthographic_projection.hpp>

#include <ngngfx_camera.hpp>
#include <ngngfx_projection.hpp>

#include <glm/gtc/matrix_transform.hpp>

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
ngngfx::orthographic_projection_t::orthographic_projection_t(
    camera_t const& camera,
    glm::vec2 const near_far_planes,
    float const aspect_ratio,
    glm::vec2 const left_right,
    glm::vec2 const bottom_top)
    : projection_t{camera, near_far_planes, aspect_ratio}
    , left_right_{left_right}
    , bottom_top_{bottom_top}
{
    calculate_view_projection_matrices();
}

void ngngfx::orthographic_projection_t::update()
{
    calculate_view_projection_matrices();
}

glm::mat4 const& ngngfx::orthographic_projection_t::projection_matrix() const
{
    return projection_matrix_;
}

glm::mat4 const&
ngngfx::orthographic_projection_t::view_projection_matrix() const
{
    return view_projection_matrix_;
}

void ngngfx::orthographic_projection_t::calculate_view_projection_matrices()
{
    projection_matrix_ = glm::orthoRH_ZO(left_right_.x,
        left_right_.y,
        bottom_top_.x,
        bottom_top_.y,
        near_far_planes_.x,
        near_far_planes_.y);

    projection_matrix_[1][1] *= -1;

    view_projection_matrix_ = projection_matrix_ * camera_->view_matrix();
}
