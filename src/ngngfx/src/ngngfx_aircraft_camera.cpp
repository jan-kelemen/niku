#include <ngngfx_aircraft_camera.hpp>

#include <ngngfx_camera.hpp>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <cmath>

ngngfx::aircraft_camera_t::aircraft_camera_t()
    : aircraft_camera_t({0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f})
{
}

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
ngngfx::aircraft_camera_t::aircraft_camera_t(glm::vec3 const& position,
    glm::vec3 const& world_up,
    glm::vec2 const& yaw_pitch)
    : camera_t{position}
    , world_up_{world_up}
    , yaw_pitch_{yaw_pitch}
{
    calculate_view_matrix();
}

glm::vec2 ngngfx::aircraft_camera_t::yaw_pitch() const { return yaw_pitch_; }

void ngngfx::aircraft_camera_t::set_yaw_pitch(glm::vec2 const& yaw_pitch)
{
    yaw_pitch_ = yaw_pitch;
}

glm::vec3 ngngfx::aircraft_camera_t::up_direction() const
{
    return up_direction_;
}

glm::vec3 ngngfx::aircraft_camera_t::front_direction() const
{
    return front_direction_;
}

glm::vec3 ngngfx::aircraft_camera_t::right_direction() const
{
    return right_direction_;
}

void ngngfx::aircraft_camera_t::update()
{
    auto const& yaw{yaw_pitch_.x};
    auto const& pitch{yaw_pitch_.y};
    glm::vec3 const front{sinf(yaw) * cosf(pitch),
        sinf(pitch),
        cosf(yaw) * cosf(pitch)};
    front_direction_ = -glm::normalize(front);

    right_direction_ = glm::normalize(glm::cross(front_direction_, world_up_));
    up_direction_ =
        glm::normalize(glm::cross(right_direction_, front_direction_));

    calculate_view_matrix();
}

glm::mat4 const& ngngfx::aircraft_camera_t::view_matrix() const
{
    return view_matrix_;
}

void ngngfx::aircraft_camera_t::calculate_view_matrix()
{
    view_matrix_ =
        glm::lookAtRH(position_, position_ + front_direction(), up_direction());
}
