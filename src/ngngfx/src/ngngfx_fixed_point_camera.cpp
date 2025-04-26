#include <ngngfx_fixed_point_camera.hpp>

#include <ngngfx_camera.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

ngngfx::fixed_point_camera_t::fixed_point_camera_t()
    : fixed_point_camera_t({0.0f, 1.0f, 0.0f},
          {0.0f, 0.0f, 0.0f},
          {0.0f, 1.0f, 0.0f})
{
}

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
ngngfx::fixed_point_camera_t::fixed_point_camera_t(glm::vec3 const& position,
    glm::vec3 const& target,
    glm::vec3 const& world_up)
    : camera_t{position}
    , world_up_{world_up}
    , target_{target}
{
    calculate_view_matrix();
}

void ngngfx::fixed_point_camera_t::set_target(glm::vec3 const& target)
{
    target_ = target;
}

glm::vec3 const& ngngfx::fixed_point_camera_t::target() const
{
    return target_;
}

void ngngfx::fixed_point_camera_t::update() { calculate_view_matrix(); }

glm::mat4 const& ngngfx::fixed_point_camera_t::view_matrix() const
{
    return view_matrix_;
}

void ngngfx::fixed_point_camera_t::calculate_view_matrix()
{
    view_matrix_ = glm::lookAtRH(position_, target_, world_up_);
}
