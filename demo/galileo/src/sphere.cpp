#include <sphere.hpp>

#include <physics_engine.hpp>
#include <render_graph.hpp>

#include <ngnphy_jolt_adapter.hpp>

// clang-format off
#include <type_traits>
#include <Jolt/Jolt.h>
// clang-format on

#include <Jolt/Core/Reference.h>
#include <Jolt/Geometry/IndexedTriangle.h>
#include <Jolt/Geometry/Triangle.h>
#include <Jolt/Math/Float3.h>
#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Real.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/MotionType.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/EActivation.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <cassert>

entt::entity galileo::create_sphere(entt::registry& registry,
    physics_engine_t& physics_engine,
    glm::vec3 const position,
    float const radius)
{
    auto& body_interface{physics_engine.body_interface()};

    JPH::BodyCreationSettings const settings{new JPH::SphereShape{radius},
        ngnphy::to_jolt(position),
        JPH::Quat::sIdentity(),
        JPH::EMotionType::Dynamic,
        object_layers::moving};

    JPH::BodyID const id{
        body_interface.CreateAndAddBody(settings, JPH::EActivation::Activate)};

    entt::entity const rv{registry.create()};
    body_interface.SetUserData(id, static_cast<uint64_t>(rv));

    registry.emplace<component::physics_t>(rv, id);
    registry.emplace<component::sphere_t>(rv);

    return rv;
}

entt::entity galileo::spawn_sphere(entt::registry& registry,
    physics_engine_t& physics_engine,
    glm::vec3 const position)
{
    if (entt::entity const entity{registry
                .view<component::sphere_t,
                    component::physics_t,
                    component::mesh_t>()
                .front()};
        entity != entt::null)
    {
        auto& body_interface{physics_engine.body_interface()};

        JPH::BodyCreationSettings const sphere_settings{
            body_interface.GetShape(
                registry.get<component::physics_t>(entity).id),
            ngnphy::to_jolt(position),
            JPH::Quat::sIdentity(),
            JPH::EMotionType::Dynamic,
            object_layers::moving};

        JPH::BodyID const id{body_interface.CreateAndAddBody(sphere_settings,
            JPH::EActivation::Activate)};

        entt::entity const rv{registry.create()};
        body_interface.SetUserData(id, static_cast<uint64_t>(rv));

        registry.emplace<component::mesh_t>(rv,
            registry.get<component::mesh_t>(entity).index);
        registry.emplace<component::physics_t>(rv, id);
        registry.emplace<component::sphere_t>(rv);

        return rv;
    }

    return entt::null;
}
