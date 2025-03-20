#include <character.hpp>

#include <cppext_numeric.hpp>

#include <ngnphy_coordinate_system.hpp>
#include <ngnphy_jolt_adapter.hpp>

#include <ngnwsi_mouse.hpp>

#include <physics_debug.hpp>
#include <physics_engine.hpp>

#include <glm/common.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>

#include <Jolt/Core/Color.h>
#include <Jolt/Core/Reference.h>
#include <Jolt/Geometry/Plane.h>
#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Real.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Character/CharacterBase.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/BackFaceMode.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/ShapeFilter.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_scancode.h>

// IWYU pragma: no_include <glm/detail/qualifier.hpp>

namespace
{
    constexpr JPH::EBackFaceMode back_face_mode{
        JPH::EBackFaceMode::CollideWithBackFaces};
    constexpr float standing_radius{0.3f};
    constexpr float standing_height{1.35f};
    constexpr float max_strength{100.0f};
    constexpr float padding{0.02f};
    constexpr float recovery_speed{1.0f};
    constexpr float predictive_contact_distance{0.1f};

    constexpr float acceleration_factor{3.09f};
} // namespace

galileo::character_t::character_t(physics_engine_t& physics_engine,
    ngnwsi::mouse_t& mouse)
    : physics_engine_{&physics_engine}
    , mouse_{&mouse}
{
    JPH::RotatedTranslatedShapeSettings const shape_settings{
        JPH::Vec3{0.0f, 0.5f * standing_height + standing_radius, 0},
        JPH::Quat::sIdentity(),
        new JPH::CapsuleShape{0.5f * standing_height, standing_radius}};
    JPH::RefConst<JPH::Shape> const standing_shape{
        shape_settings.Create().Get()};

    JPH::Ref<JPH::CharacterVirtualSettings> const settings{
        new JPH::CharacterVirtualSettings};
    settings->mMaxSlopeAngle = max_slope_angle;
    settings->mMaxStrength = max_strength;
    settings->mShape = standing_shape;
    settings->mBackFaceMode = back_face_mode;
    settings->mCharacterPadding = padding;
    settings->mPenetrationRecoverySpeed = recovery_speed;
    settings->mPredictiveContactDistance = predictive_contact_distance;
    // Accept contacts that touch the lower sphere of the capsule
    settings->mSupportingVolume =
        JPH::Plane{JPH::Vec3::sAxisY(), -standing_radius};
    settings->mEnhancedInternalEdgeRemoval = false;
    settings->mInnerBodyShape = nullptr;
    settings->mInnerBodyLayer = object_layers::moving;

    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    physics_entity_ = new JPH::CharacterVirtual{settings,
        JPH::RVec3::sZero(),
        JPH::Quat::sIdentity(),
        0,
        &physics_engine.physics_system()};
}

[[nodiscard]] galileo::character_action_t galileo::character_t::handle_event(
    SDL_Event const& event)
{
    if (!mouse_->captured())
    {
        return character_action_t::none;
    }

    if (event.type == SDL_EVENT_MOUSE_MOTION)
    {
        auto const& mouse_offset{mouse_->relative_offset()};

        auto new_rot{glm::normalize(
            glm::quat{glm::vec3{0.0f,
                glm::radians(cppext::as_fp(-mouse_offset.x) * 0.01f),
                0.0f}} *
            rotation())};

        physics_entity_->SetRotation(ngnphy::to_jolt(new_rot));
    }
    else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
        event.button.button == SDL_BUTTON_LEFT)
    {
        return character_action_t::select_body;
    }

    return character_action_t::none;
}

void galileo::character_t::set_contact_listener(
    JPH::CharacterContactListener* const listener)
{
    physics_entity_->SetListener(listener);
}

void galileo::character_t::update(float const delta_time)
{
    auto& system{physics_engine_->physics_system()};

    {
        glm::vec3 const gravity{ngnphy::to_glm(system.GetGravity())};

        glm::vec3 desired_direction{gravity};
        glm::quat const rot{ngnphy::to_glm(physics_entity_->GetRotation())};

        int keyboard_state_length; // NOLINT
        bool const* const keyboard_state{
            SDL_GetKeyboardState(&keyboard_state_length)};

        bool has_input{false};
        if (keyboard_state[SDL_SCANCODE_A])
        {
            desired_direction +=
                rot * ngnphy::coordinate_system_t::left * acceleration_factor;
            has_input = true;
        }

        if (keyboard_state[SDL_SCANCODE_D])
        {
            desired_direction +=
                rot * ngnphy::coordinate_system_t::right * acceleration_factor;
            has_input = true;
        }

        if (keyboard_state[SDL_SCANCODE_W])
        {
            desired_direction +=
                rot * ngnphy::coordinate_system_t::front * acceleration_factor;
            has_input = true;
        }

        if (keyboard_state[SDL_SCANCODE_S])
        {
            desired_direction +=
                rot * ngnphy::coordinate_system_t::back * acceleration_factor;
            has_input = true;
        }

        if (has_input)
        {
            physics_entity_->SetLinearVelocity(
                0.5f * physics_entity_->GetLinearVelocity() +
                0.5f * ngnphy::to_jolt(desired_direction));
        }
        else if (physics_entity_->GetGroundState() ==
            JPH::CharacterBase::EGroundState::OnGround)
        {
            auto const slow_down{
                glm::mix(ngnphy::to_glm(physics_entity_->GetLinearVelocity()),
                    gravity,
                    0.05f)};
            physics_entity_->SetLinearVelocity(ngnphy::to_jolt(slow_down));
        }
        else
        {
            physics_entity_->SetLinearVelocity(
                ngnphy::to_jolt(desired_direction));
        }
    }

    JPH::CharacterVirtual::ExtendedUpdateSettings update_settings;
    update_settings.mStickToFloorStepDown = -physics_entity_->GetUp() *
        update_settings.mStickToFloorStepDown.Length();
    update_settings.mWalkStairsStepUp =
        physics_entity_->GetUp() * update_settings.mWalkStairsStepUp.Length();

    physics_entity_->ExtendedUpdate(delta_time,
        -physics_entity_->GetUp() * system.GetGravity().Length(),
        update_settings,
        system.GetDefaultBroadPhaseLayerFilter(object_layers::moving),
        system.GetDefaultLayerFilter(object_layers::moving),
        {},
        {},
        physics_engine_->allocator());
}

void galileo::character_t::debug(physics_debug_t* const physics_debug)
{
    JPH::RMat44 const com{physics_entity_->GetCenterOfMassTransform()};
    physics_entity_->GetShape()->Draw(physics_debug,
        com,
        JPH::Vec3::sReplicate(1.0f),
        JPH::Color::sGreen,
        false,
        true);

    JPH::Quat const rot{physics_entity_->GetRotation()};

    JPH::Vec3 const front_direction{
        rot * ngnphy::to_jolt(ngnphy::coordinate_system_t::front)};
    JPH::RVec3 const pos{physics_entity_->GetCenterOfMassPosition()};

    physics_debug->DrawArrow(pos, pos + front_direction, JPH::Color::sRed, .1f);
}

glm::vec3 galileo::character_t::position() const
{
    return ngnphy::to_glm(physics_entity_->GetPosition());
}

void galileo::character_t::set_position(glm::vec3 position)
{
    auto& system{physics_engine_->physics_system()};

    physics_entity_->SetPosition(ngnphy::to_jolt(position));
    physics_entity_->RefreshContacts(
        system.GetDefaultBroadPhaseLayerFilter(object_layers::moving),
        system.GetDefaultLayerFilter(object_layers::moving),
        {},
        {},
        physics_engine_->allocator());
}

glm::vec3 galileo::character_t::center_of_mass() const
{
    return ngnphy::to_glm(physics_entity_->GetCenterOfMassPosition());
}

glm::quat galileo::character_t::rotation() const
{
    return ngnphy::to_glm(physics_entity_->GetRotation());
}

glm::mat4 galileo::character_t::world_transform() const
{
    return ngnphy::to_glm(physics_entity_->GetWorldTransform());
}
