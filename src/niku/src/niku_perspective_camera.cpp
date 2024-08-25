#include <niku_perspective_camera.hpp>

#include <niku_camera.hpp>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>

#include <cmath>

niku::perspective_camera::perspective_camera()
    : perspective_camera({0.0f, 0.0f, 0.0f},
          16.0f / 9.0f,
          45.0f,
          {0.0f, 1.0f, 0.0f},
          {0.1f, 1000.0f},
          {0.0f, 0.0f})
{
}

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
niku::perspective_camera::perspective_camera(glm::vec3 const& position,
    float const aspect_ratio,
    float const fov,
    glm::vec3 const& world_up,
    glm::vec2 const& near_far_planes,
    glm::vec2 const& yaw_pitch)
    : camera(position, aspect_ratio)
    , world_up_{world_up}
    , near_far_planes_{near_far_planes}
    , yaw_pitch_{yaw_pitch}
    , fov_{fov}
{
    calculate_view_projection_matrices();
}

glm::vec2 const& niku::perspective_camera::yaw_pitch() const
{
    return yaw_pitch_;
}

void niku::perspective_camera::set_yaw_pitch(glm::vec2 const& yaw_pitch)
{
    yaw_pitch_ = yaw_pitch;
}

void niku::perspective_camera::set_near_far(glm::vec2 const& near_far)
{
    near_far_planes_ = near_far;
}

glm::vec3 const& niku::perspective_camera::up_direction() const
{
    return up_direction_;
}

glm::vec3 const& niku::perspective_camera::front_direction() const
{
    return front_direction_;
}

glm::vec3 const& niku::perspective_camera::right_direction() const
{
    return right_direction_;
}

void niku::perspective_camera::update()
{
    auto const& yaw{yaw_pitch_.x};
    auto const& pitch{yaw_pitch_.y};
    glm::vec3 const front{cosf(glm::radians(yaw)) * cosf(glm::radians(pitch)),
        sinf(glm::radians(pitch)),
        sinf(glm::radians(yaw)) * cosf(glm::radians(pitch))};
    front_direction_ = glm::normalize(front);

    right_direction_ = glm::normalize(glm::cross(front_direction_, world_up_));
    up_direction_ =
        glm::normalize(glm::cross(right_direction_, front_direction_));

    calculate_view_projection_matrices();
}

glm::mat4 const& niku::perspective_camera::view_matrix() const
{
    return view_matrix_;
}

glm::mat4 const& niku::perspective_camera::projection_matrix() const
{
    return projection_matrix_;
}

glm::mat4 const& niku::perspective_camera::view_projection_matrix() const
{
    return view_projection_matrix_;
}

void niku::perspective_camera::calculate_view_projection_matrices()
{
    view_matrix_ =
        glm::lookAtRH(position_, position_ + front_direction_, up_direction_);

    projection_matrix_ = glm::perspectiveRH_ZO(glm::radians(fov_),
        aspect_ratio_,
        near_far_planes_.x,
        near_far_planes_.y);

    projection_matrix_[1][1] *= -1;

    view_projection_matrix_ = view_matrix_ * projection_matrix_;
}
