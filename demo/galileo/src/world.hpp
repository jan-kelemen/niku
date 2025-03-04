#ifndef GALILEO_WORLD_INCLUDED
#define GALILEO_WORLD_INCLUDED

#include <navmesh.hpp>

#include <ngnphy_jolt.hpp> // IWYU pragma: keep

#include <Jolt/Physics/Body/BodyID.h>

#include <glm/vec3.hpp>

#include <optional>

// IWYU pragma: no_include <recastnavigation/DetourNavMesh.h>

namespace galileo
{
    class physics_engine_t;
} // namespace galileo

namespace galileo
{
    class [[nodiscard]] world_t final
    {
    public:
        explicit world_t(physics_engine_t& physics_engine);

        world_t(world_t const&) = delete;

        world_t(world_t&&) noexcept = default;

    public:
        ~world_t() = default;

    public:
        [[nodiscard]] std::optional<JPH::BodyID> cast_ray(glm::vec3 from,
            glm::vec3 direction_and_reach);

        [[nodiscard]] navigation_mesh_query_ptr_t get_navigation_query();

        [[nodiscard]] dtNavMesh const& get_navigation_mesh() const;

        void update_navigation_mesh(navigation_mesh_ptr_t navigation_mesh);

    public:
        world_t& operator=(world_t const&) = delete;

        world_t& operator=(world_t&&) noexcept = delete;

    private:
        physics_engine_t* physics_engine_;
        navigation_mesh_ptr_t navigation_mesh_;
    };
} // namespace galileo
#endif
