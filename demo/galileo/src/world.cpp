#include <world.hpp>

#include <physics_engine.hpp>

#include <ngnphy_jolt_adapter.hpp>

#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <glm/vec3.hpp>

galileo::world_t::world_t(physics_engine_t& physics_engine)
    : physics_engine_{&physics_engine}
{
}

std::optional<JPH::BodyID> galileo::world_t::cast_ray(glm::vec3 const from,
    glm::vec3 const direction_and_reach)
{
    JPH::RRayCast ray{ngnphy::to_jolt(from),
        ngnphy::to_jolt(direction_and_reach)};

    auto& system{physics_engine_->physics_system()};

    JPH::RayCastResult hit;
    bool const had_hit{system.GetNarrowPhaseQuery().CastRay(ray,
        hit,
        system.GetDefaultBroadPhaseLayerFilter(object_layers::moving),
        system.GetDefaultLayerFilter(object_layers::moving))};
    if (had_hit)
    {
        return hit.mBodyID;
    }

    return std::nullopt;
}

galileo::navigation_mesh_query_ptr_t galileo::world_t::get_navigation_query()
{
    return create_query(navigation_mesh_.get());
}

void galileo::world_t::update_navigation_mesh(
    navigation_mesh_ptr_t navigation_mesh)
{
    navigation_mesh_ = std::move(navigation_mesh);
}
