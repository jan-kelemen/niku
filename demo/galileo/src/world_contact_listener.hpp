#ifndef GALILEO_WORLD_CONTACT_LISTENER_INCLUDED
#define GALILEO_WORLD_CONTACT_LISTENER_INCLUDED

#include <entt/entt.hpp>

#include <Jolt/Jolt.h> // IWYU pragma: keep

#include <Jolt/Physics/Collision/ContactListener.h>

namespace ngnscr
{
    class scripting_engine_t;
} // namespace ngnscr

namespace galileo
{
    class [[nodiscard]] world_contact_listener_t final
        : public JPH::ContactListener
    {
    public:
        world_contact_listener_t(ngnscr::scripting_engine_t& scripting_engine,
            entt::registry& registry);

        world_contact_listener_t(world_contact_listener_t const&) = delete;

        world_contact_listener_t(world_contact_listener_t&&) noexcept = delete;

    public:
        ~world_contact_listener_t() override = default;

    public:
        world_contact_listener_t& operator=(
            world_contact_listener_t const&) = delete;

        world_contact_listener_t& operator=(
            world_contact_listener_t&&) noexcept = delete;

    private:
        void OnContactPersisted(const JPH::Body& inBody1,
            const JPH::Body& inBody2,
            const JPH::ContactManifold& inManifold,
            JPH::ContactSettings& ioSettings) override;

    private:
        ngnscr::scripting_engine_t* scripting_engine_;
        entt::registry* registry_;
    };
} // namespace galileo

#endif
