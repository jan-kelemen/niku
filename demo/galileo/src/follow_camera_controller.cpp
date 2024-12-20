#include <follow_camera_controller.hpp>

#include <character.hpp>

#include <ngngfx_perspective_camera.hpp>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <cmath>

// IWYU pragma: no_include <glm/detail/qualifier.hpp>
// IWYU pragma: no_include <glm/mat3x3.hpp>

galileo::follow_camera_controller_t::follow_camera_controller_t(
    ngngfx::perspective_camera_t& camera)
    : camera_{&camera}
{
}

bool galileo::follow_camera_controller_t::update(character_t const& character)
{
    float yaw, pitch, roll; // NOLINT
    glm::extractEulerAngleYXZ(glm::mat4_cast(character.rotation()),
        yaw,
        pitch,
        roll);

    camera_->set_yaw_pitch(
        glm::vec2{glm::degrees(yaw) + 90.0f, glm::degrees(pitch)} * -1.0f);
    camera_->update();

    glm::vec3 const position{character.position() +
        camera_->up_direction() * 1.76f + camera_->front_direction() * -5.0f};

    camera_->set_position(position);
    camera_->update();

    return false;
}
