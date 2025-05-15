#include <ngngfx_perspective_projection.hpp>

#include <ngngfx_projection.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/trigonometric.hpp>

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
ngngfx::perspective_projection_t::perspective_projection_t(
    glm::vec2 const near_far_planes,
    float const aspect_ratio,
    float const fov,
    bool const invert_y)
    : projection_t{near_far_planes, invert_y}
    , fov_{fov}
    , aspect_ratio_{aspect_ratio}
{
}

void ngngfx::perspective_projection_t::set_aspect_ratio(float aspect_ratio)
{
    aspect_ratio_ = aspect_ratio;
}

float ngngfx::perspective_projection_t::aspect_ratio() const
{
    return aspect_ratio_;
}

void ngngfx::perspective_projection_t::update(glm::mat4 const& view_matrix)
{
    calculate_view_projection_matrices(view_matrix);
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

void ngngfx::perspective_projection_t::calculate_view_projection_matrices(
    glm::mat4 const& view_matrix)
{
    projection_matrix_ = glm::perspectiveRH_ZO(glm::radians(fov_),
        aspect_ratio_,
        near_far_planes_.x,
        near_far_planes_.y);

    if (invert_y_)
    {
        projection_matrix_[1][1] *= -1;
    }

    view_projection_matrix_ = projection_matrix_ * view_matrix;
}
