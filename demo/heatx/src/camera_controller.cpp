#include <camera_controller.hpp>

#include <cppext_numeric.hpp>

#include <ngngfx_aircraft_camera.hpp>

#include <ngnwsi_mouse.hpp>

#include <imgui.h>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_scancode.h>

#include <algorithm>
#include <cmath>

heatx::camera_controller_t::camera_controller_t(
    ngngfx::aircraft_camera_t& camera,
    ngnwsi::mouse_t& mouse)
    : camera_{&camera}
    , mouse_{&mouse}
{
}

void heatx::camera_controller_t::handle_event(SDL_Event const& event)
{
    if (event.type == SDL_EVENT_MOUSE_MOTION && mouse_->captured())
    {
        auto const& yaw_pitch{camera_->yaw_pitch()};
        auto const& mouse_offset{mouse_->relative_offset()};

        auto const yaw{yaw_pitch.x +
            glm::radians(cppext::as_fp(-mouse_offset.x) * mouse_sensitivity_)};
        auto const pitch{yaw_pitch.y +
            glm::radians(cppext::as_fp(mouse_offset.y) * mouse_sensitivity_)};

        camera_->set_yaw_pitch({fmodf(yaw, glm::two_pi<float>()),
            std::clamp(pitch, glm::radians(-85.0f), glm::radians(85.0f))});

        update_needed_ = true;
    }
    else if (event.type == SDL_EVENT_KEY_DOWN &&
        event.key.scancode == SDL_SCANCODE_F3)
    {
        mouse_->set_capture(!mouse_->captured());
    }
}

bool heatx::camera_controller_t::update(float const delta_time)
{
    {
        int keyboard_state_length; // NOLINT
        bool const* const keyboard_state{
            SDL_GetKeyboardState(&keyboard_state_length)};

        velocity_ = {0.0f, 0.0f, 0.0f};

        if (keyboard_state[SDL_SCANCODE_LEFT] || keyboard_state[SDL_SCANCODE_A])
        {
            velocity_ -= velocity_factor_ * camera_->right_direction();
        }

        if (keyboard_state[SDL_SCANCODE_RIGHT] ||
            keyboard_state[SDL_SCANCODE_D])
        {
            velocity_ += velocity_factor_ * camera_->right_direction();
        }

        if (keyboard_state[SDL_SCANCODE_UP] || keyboard_state[SDL_SCANCODE_W])
        {
            velocity_ += velocity_factor_ * camera_->front_direction();
        }

        if (keyboard_state[SDL_SCANCODE_DOWN] || keyboard_state[SDL_SCANCODE_S])
        {
            velocity_ -= velocity_factor_ * camera_->front_direction();
        }

        update_needed_ = true;
    }

    if (update_needed_)
    {
        camera_->set_position(camera_->position() + velocity_ * delta_time);
        camera_->update();

        update_needed_ = false;
        return true;
    }

    return false;
}

void heatx::camera_controller_t::draw_imgui()
{
    ImGui::Begin("Camera");

    if (glm::vec2 yp{camera_->yaw_pitch()};
        ImGui::SliderFloat("Yaw", &yp.r, 0.0f, glm::two_pi<float>()) ||
        ImGui::SliderFloat("Pitch", &yp.g, -85.0f, 85.0f))
    {
        camera_->set_yaw_pitch(yp);
    }

    if (glm::vec3 p{camera_->position()};
        ImGui::SliderFloat3("Position", glm::value_ptr(p), -100.0f, 100.f))
    {
        camera_->set_position(p);
    }

    ImGui::SliderFloat("Mouse sensitivity", &mouse_sensitivity_, 0.01f, 10.0f);
    ImGui::SliderFloat("Velocity factor", &velocity_factor_, 0.01f, 5.0f);
    ImGui::End();
}
