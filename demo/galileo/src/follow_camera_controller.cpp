#include <follow_camera_controller.hpp>

#include <character.hpp>

#include <cppext_numeric.hpp>

#include <ngngfx_perspective_camera.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

#include <spdlog/spdlog.h>

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
