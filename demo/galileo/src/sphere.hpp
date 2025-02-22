#ifndef GALILEO_SPHERE_INCLUDED
#define GALILEO_SPHERE_INCLUDED

#include <navmesh.hpp>

#include <entt/entt.hpp>

#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

namespace galileo
{
    class physics_engine_t;
} // namespace galileo

namespace galileo
{
    entt::entity create_sphere(entt::registry& registry,
        physics_engine_t& physics_engine,
        glm::vec3 position,
        float radius);

    entt::entity spawn_sphere(entt::registry& registry,
        physics_engine_t& physics_engine,
        glm::vec3 position);
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
    };
} // namespace galileo::component

#endif
