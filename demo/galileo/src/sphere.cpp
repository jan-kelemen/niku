#include <sphere.hpp>

#include <navmesh.hpp>
#include <physics_engine.hpp>
#include <scene_graph.hpp>
#include <scripting.hpp>

#include <ngnphy_jolt_adapter.hpp>

#include <ngnscr_scripting_engine.hpp>

#include <angelscript.h>

#include <Jolt/Jolt.h> // IWYU pragma: keep

#include <Jolt/Core/Reference.h>
#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/MotionType.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/EActivation.h>

#include <spdlog/spdlog.h>

#include <cassert>
#include <cstdint>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

// IWYU pragma: no_include <fmt/base.h>
// IWYU pragma: no_include <fmt/format.h>
// IWYU pragma: no_include <expected>

entt::entity galileo::create_sphere(entt::registry& registry,
    physics_engine_t& physics_engine,
    ngnscr::scripting_engine_t& scripting_engine,
    glm::vec3 const position,
    float const radius)
{
    auto& body_interface{physics_engine.body_interface()};

    auto shape{std::make_unique<JPH::SphereShape>(radius)};
    shape->SetDensity(500.0f);

    JPH::BodyCreationSettings settings{shape.get(),
        ngnphy::to_jolt(position),
        JPH::Quat::sIdentity(),
        JPH::EMotionType::Dynamic,
        object_layers::moving};
    settings.mMaxLinearVelocity = 1.42f;

    shape.release(); // NOLINT

    JPH::BodyID const id{
        body_interface.CreateAndAddBody(settings, JPH::EActivation::Activate)};

    entt::entity const rv{registry.create()};
    body_interface.SetUserData(id, static_cast<uint64_t>(rv));

    registry.emplace<component::physics_t>(rv, id);
    registry.emplace<component::sphere_t>(rv);

    asIScriptModule const* const mod{
        scripting_engine.engine().GetModule("MyModule")};
    assert(mod);

    if (auto scripts{
            component::create_sphere_scripts(rv, scripting_engine, *mod)})
    {
        registry.emplace<component::scripts_t>(rv, std::move(*scripts));
    }
    return rv;
}

entt::entity galileo::spawn_sphere(entt::registry& registry,
    physics_engine_t& physics_engine,
    ngnscr::scripting_engine_t& scripting_engine,
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

        asIScriptModule const* const mod{
            scripting_engine.engine().GetModule("MyModule")};
        assert(mod);

        if (auto scripts{
                component::create_sphere_scripts(rv, scripting_engine, *mod)})
        {
            registry.emplace<component::scripts_t>(rv, std::move(*scripts));
        }
        return rv;
    }

    return entt::null;
}

void galileo::move_spheres(entt::registry& registry,
    physics_engine_t& physics_engine)
{
    auto& body_interface{physics_engine.body_interface()};
    std::vector<entt::entity> to_remove;
    for (auto const entity :
        registry.view<component::sphere_path_t, component::physics_t>())
    {
        auto& path{registry.get<component::sphere_path_t>(entity)};

        auto const physics_id{registry.get<component::physics_t>(entity).id};

        glm::vec3 const current{
            ngnphy::to_glm(body_interface.GetPosition(physics_id))};

        std::vector<path_point_t> corners{
            find_corners(path.navmesh_path, current, 2)};
        if (std::empty(corners))
        {
            spdlog::error("Can't find next corner to steer to");
            to_remove.push_back(entity);
            continue;
        }

        auto const error{corners.front().vertex - current};

        body_interface.AddForce(physics_id, 500.0f * ngnphy::to_jolt(error));
    }

    registry.remove<component::sphere_path_t>(to_remove.cbegin(),
        to_remove.cend());
}
