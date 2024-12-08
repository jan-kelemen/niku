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

gltfviewer::camera_controller_t::camera_controller_t(
    ngngfx::perspective_camera_t& camera,
    ngnwsi::mouse_t& mouse)
    : camera_{&camera}
    , mouse_{&mouse}
{
}

void gltfviewer::camera_controller_t::handle_event(SDL_Event const& event)
{
    if (event.type == SDL_MOUSEMOTION && mouse_->captured())
    {
        auto const& yaw_pitch{camera_->yaw_pitch()};
        auto const& mouse_offset{mouse_->relative_offset()};

        auto const yaw{
            yaw_pitch.x + cppext::as_fp(mouse_offset.x) * mouse_sensitivity_};
        auto const pitch{
            yaw_pitch.y + cppext::as_fp(-mouse_offset.y) * mouse_sensitivity_};

        camera_->set_yaw_pitch(
            {fmodf(yaw, 360), std::clamp(pitch, -85.0f, 85.0f)});

        update_needed_ = true;
    }
    else if (event.type == SDL_KEYDOWN &&
        event.key.keysym.scancode == SDL_SCANCODE_F3)
    {
        mouse_->set_capture(!mouse_->captured());
    }
}

bool gltfviewer::camera_controller_t::update(float const delta_time)
{
    {
        int keyboard_state_length; // NOLINT
        uint8_t const* const keyboard_state{
            SDL_GetKeyboardState(&keyboard_state_length)};

        velocity_ = {0.0f, 0.0f, 0.0f};

        if (keyboard_state[SDL_SCANCODE_LEFT] != 0 ||
            keyboard_state[SDL_SCANCODE_A] != 0)
        {
            velocity_ -= velocity_factor_ * camera_->right_direction();
        }

        if (keyboard_state[SDL_SCANCODE_RIGHT] != 0 ||
            keyboard_state[SDL_SCANCODE_D] != 0)
        {
            velocity_ += velocity_factor_ * camera_->right_direction();
        }

        if (keyboard_state[SDL_SCANCODE_UP] != 0 ||
            keyboard_state[SDL_SCANCODE_W] != 0)
        {
            velocity_ += velocity_factor_ * camera_->front_direction();
        }

        if (keyboard_state[SDL_SCANCODE_DOWN] != 0 ||
            keyboard_state[SDL_SCANCODE_S] != 0)
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

void gltfviewer::camera_controller_t::draw_imgui()
{
    ImGui::Begin("Camera");
    ImGui::SliderFloat("Mouse sensitivity", &mouse_sensitivity_, 0.01f, 5.0f);
    ImGui::SliderFloat("Velocity factor", &velocity_factor_, 0.01f, 5.0f);
    ImGui::End();
}
