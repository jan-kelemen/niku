#include <ngngfx_orthographic_projection.hpp>

#include <ngngfx_projection.hpp>

#include <glm/gtc/matrix_transform.hpp>

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
ngngfx::orthographic_projection_t::orthographic_projection_t(
    glm::vec2 const near_far_planes,
    float const aspect_ratio,
    glm::vec2 const left_right,
    glm::vec2 const bottom_top)
    : projection_t{near_far_planes, aspect_ratio}
    , left_right_{left_right}
    , bottom_top_{bottom_top}
{
}

void ngngfx::orthographic_projection_t::set_left_right(
    glm::vec2 const left_right)
{
    left_right_ = left_right;
}

glm::vec2 ngngfx::orthographic_projection_t::left_right() const
{
    return left_right_;
}

void ngngfx::orthographic_projection_t::set_bottom_top(
    glm::vec2 const bottom_top)
{
    bottom_top_ = bottom_top;
}

glm::vec2 ngngfx::orthographic_projection_t::bottom_top() const
{
    return bottom_top_;
}

void ngngfx::orthographic_projection_t::update(glm::mat4 const& view_matrix)
{
    calculate_view_projection_matrices(view_matrix);
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

void ngngfx::orthographic_projection_t::calculate_view_projection_matrices(
    glm::mat4 const& view_matrix)
{
    projection_matrix_ = glm::orthoRH_ZO(left_right_.x,
        left_right_.y,
        bottom_top_.x,
        bottom_top_.y,
        near_far_planes_.x,
        near_far_planes_.y);

    projection_matrix_[1][1] *= -1;

    view_projection_matrix_ = projection_matrix_ * view_matrix;
}
