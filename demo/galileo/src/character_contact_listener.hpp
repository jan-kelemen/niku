#ifndef GALILEO_CHARACTER_CONTACT_LISTENER_INCLUDED
#define GALILEO_CHARACTER_CONTACT_LISTENER_INCLUDED

#include <entt/entt.hpp>

#include <ngnphy_jolt.hpp> // IWYU pragma: keep

#include <Jolt/Math/MathTypes.h>
#include <Jolt/Math/Real.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>

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
    class character_contact_listener_t final
        : public JPH::CharacterContactListener
    {
    public:
        explicit character_contact_listener_t(physics_engine_t& physics_engine,
            ngnscr::scripting_engine_t& scripting_engine,
            entt::registry& registry);

        character_contact_listener_t(
            character_contact_listener_t const&) = default;

        character_contact_listener_t(
            character_contact_listener_t&&) noexcept = default;

    public:
        ~character_contact_listener_t() override = default;

    public:
        character_contact_listener_t& operator=(
            character_contact_listener_t const&) = default;
        character_contact_listener_t& operator=(
            character_contact_listener_t&&) noexcept = default;

    private:
        void OnContactAdded(JPH::CharacterVirtual const* inCharacter,
            JPH::BodyID const& inBodyID2,
            JPH::SubShapeID const& inSubShapeID2,
            JPH::RVec3Arg inContactPosition,
            JPH::Vec3Arg inContactNormal,
            JPH::CharacterContactSettings& ioSettings) override;

    private:
        physics_engine_t* physics_engine_;
        ngnscr::scripting_engine_t* scripting_engine_;
        entt::registry* registry_;
    };

} // namespace galileo

#endif
