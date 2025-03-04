#ifndef GALILEO_SPHERE_INCLUDED
#define GALILEO_SPHERE_INCLUDED

#include <navmesh.hpp>

#include <entt/entt.hpp>

#include <glm/vec3.hpp>

// IWYU pragma: no_include <recastnavigation/DetourNavMeshQuery.h>
// IWYU pragma: no_include <memory>

namespace ngnscr
{
    class scripting_engine_t;
} // namespace ngnscr

namespace galileo
{
    class physics_engine_t;
} // namespace galileo

namespace galileo
{
    entt::entity create_sphere(entt::registry& registry,
        physics_engine_t& physics_engine,
        ngnscr::scripting_engine_t& scripting_engine,
        glm::vec3 position,
        float radius);

    entt::entity spawn_sphere(entt::registry& registry,
        physics_engine_t& physics_engine,
        ngnscr::scripting_engine_t& scripting_engine,
        glm::vec3 position);

    void move_spheres(entt::registry& registry,
        physics_engine_t& physics_engine,
        float delta_time);
} // namespace galileo

namespace galileo::component
{
    struct [[nodiscard]] sphere_t final
    {
    };

    struct [[nodiscard]] sphere_path_t final
    {
        navigation_mesh_query_ptr_t query;
        path_iterator_t iterator;

        glm::vec3 error{};
        glm::vec3 integral{};
    };
} // namespace galileo::component

#endif
