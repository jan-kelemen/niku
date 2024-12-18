#include <character.hpp>

#include <ngnphy_jolt_adapter.hpp>

#include <physics_debug.hpp>
#include <physics_engine.hpp>

#include <Jolt/Jolt.h> // IWYU pragma: keep

#include <Jolt/Core/Color.h>
#include <Jolt/Core/Reference.h>
#include <Jolt/Geometry/Plane.h>
#include <Jolt/Math/Math.h>
#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Real.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/BackFaceMode.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/ShapeFilter.h>
#include <Jolt/Physics/PhysicsSystem.h>

namespace
{
    constexpr JPH::EBackFaceMode sBackFaceMode{
        JPH::EBackFaceMode::CollideWithBackFaces};
    constexpr float cCharacterRadiusStanding{0.3f};
    constexpr float cCharacterHeightStanding{1.35f};
    constexpr float sMaxSlopeAngle{JPH::DegreesToRadians(45.0f)};
    constexpr float sMaxStrength{100.0f};
    constexpr float sCharacterPadding{0.02f};
    constexpr float sPenetrationRecoverySpeed{1.0f};
    constexpr float sPredictiveContactDistance{0.1f};
    constexpr bool sEnhancedInternalEdgeRemoval{false};
} // namespace

galileo::character_t::character_t(physics_engine_t& physics_engine)
    : physics_engine_{&physics_engine}
{
    JPH::RotatedTranslatedShapeSettings const shape_settings{
        JPH::Vec3{0.0f,
            0.5f * cCharacterHeightStanding + cCharacterRadiusStanding,
            0},
        JPH::Quat::sIdentity(),
        new JPH::CapsuleShape{0.5f * cCharacterHeightStanding,
            cCharacterRadiusStanding}};
    JPH::RefConst<JPH::Shape> const standing_shape{
        shape_settings.Create().Get()};

    JPH::Ref<JPH::CharacterVirtualSettings> const settings{
        new JPH::CharacterVirtualSettings};
    settings->mMaxSlopeAngle = sMaxSlopeAngle;
    settings->mMaxStrength = sMaxStrength;
    settings->mShape = standing_shape;
    settings->mBackFaceMode = sBackFaceMode;
    settings->mCharacterPadding = sCharacterPadding;
    settings->mPenetrationRecoverySpeed = sPenetrationRecoverySpeed;
    settings->mPredictiveContactDistance = sPredictiveContactDistance;
    // Accept contacts that touch the lower sphere of the capsule
    settings->mSupportingVolume =
        JPH::Plane{JPH::Vec3::sAxisY(), -cCharacterRadiusStanding};
    settings->mEnhancedInternalEdgeRemoval = sEnhancedInternalEdgeRemoval;
    settings->mInnerBodyShape = nullptr;
    settings->mInnerBodyLayer = object_layers::moving;

    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    physics_entity_ = new JPH::CharacterVirtual{settings,
        JPH::RVec3::sZero(),
        JPH::Quat::sIdentity(),
        0,
        &physics_engine.physics_system()};
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

void galileo::character_t::update(float const delta_time)
{
    auto& system{physics_engine_->physics_system()};

    JPH::CharacterVirtual::ExtendedUpdateSettings update_settings;
    update_settings.mStickToFloorStepDown = -physics_entity_->GetUp() *
        update_settings.mStickToFloorStepDown.Length();
    update_settings.mWalkStairsStepUp =
        physics_entity_->GetUp() * update_settings.mWalkStairsStepUp.Length();

    physics_entity_->SetLinearVelocity(system.GetGravity());

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
}
