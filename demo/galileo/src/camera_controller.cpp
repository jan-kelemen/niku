#include <camera_controller.hpp>

#include <cppext_numeric.hpp>

#include <ngngfx_perspective_camera.hpp>

#include <ngnwsi_mouse.hpp>

#include <imgui.h>

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_scancode.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

galileo::camera_controller_t::camera_controller_t(
    ngngfx::perspective_camera_t& camera,
    ngnwsi::mouse_t& mouse)
    : camera_{&camera}
    , mouse_{&mouse}
{
}

void galileo::camera_controller_t::handle_event(SDL_Event const& event,
    float const delta_time)
{
    if (event.type == SDL_MOUSEMOTION && mouse_->captured())
    {
        auto const& yaw_pitch{camera_->yaw_pitch()};
        auto const& mouse_offset{mouse_->relative_offset()};

        auto const yaw{
            yaw_pitch.x + cppext::as_fp(mouse_offset.x) * delta_time};
        auto const pitch{
            yaw_pitch.y + cppext::as_fp(-mouse_offset.y) * delta_time};

        camera_->set_yaw_pitch(
            {fmodf(yaw, 360), std::clamp(pitch, -85.0f, 85.0f)});

        update_needed_ = true;
    }
}

bool galileo::camera_controller_t::update(float const delta_time)
{
    {
        int keyboard_state_length; // NOLINT
        uint8_t const* const keyboard_state{
            SDL_GetKeyboardState(&keyboard_state_length)};

        velocity_ = {0.0f, 0.0f, 0.0f};

        if (keyboard_state[SDL_SCANCODE_LEFT] != 0)
        {
            velocity_ -= velocity_factor_ * camera_->right_direction();
            update_needed_ = true;
        }

        if (keyboard_state[SDL_SCANCODE_RIGHT] != 0)
        {
            velocity_ += velocity_factor_ * camera_->right_direction();
            update_needed_ = true;
        }

        if (keyboard_state[SDL_SCANCODE_UP] != 0)
        {
            velocity_ += velocity_factor_ * camera_->front_direction();
            update_needed_ = true;
        }

        if (keyboard_state[SDL_SCANCODE_DOWN] != 0)
        {
            velocity_ -= velocity_factor_ * camera_->front_direction();
            update_needed_ = true;
        }
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

void galileo::camera_controller_t::draw_imgui()
{
    ImGui::Begin("Camera");
    ImGui::SliderFloat("Mouse sensitivity", &mouse_sensitivity_, 0.01f, 5.0f);
    ImGui::SliderFloat("Velocity factor", &velocity_factor_, 0.01f, 5.0f);
    ImGui::End();
}
