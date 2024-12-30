#ifndef GALILEO_CHARACTER_INCLUDED
#define GALILEO_CHARACTER_INCLUDED

#include <Jolt/Jolt.h> // IWYU pragma: keep

#include <Jolt/Core/Reference.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>

#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

union SDL_Event;

namespace ngnwsi
{
    class mouse_t;
} // namespace ngnwsi

namespace galileo
{
    class physics_engine_t;
    class physics_debug_t;
} // namespace galileo

namespace galileo
{
    class [[nodiscard]] character_t final
    {
    public:
        explicit character_t(physics_engine_t& physics_engine,
            ngnwsi::mouse_t& mouse);

    public:
        void handle_event(SDL_Event const& event, float delta_time);

        void update(float delta_time);

        void debug(physics_debug_t* physics_debug);

        [[nodiscard]] glm::vec3 position() const;

        void set_position(glm::vec3 position);

        [[nodiscard]] glm::quat rotation() const;

        [[nodiscard]] glm::mat4 world_transform() const;

    private:
        class contact_listener_t final : public JPH::CharacterContactListener
        {
        public:
            explicit contact_listener_t(physics_engine_t& physics_engine);

            contact_listener_t(contact_listener_t const&) = default;

            contact_listener_t(contact_listener_t&&) noexcept = default;

        public:
            ~contact_listener_t() override = default;

        public:
            contact_listener_t& operator=(contact_listener_t const&) = default;
            contact_listener_t& operator=(
                contact_listener_t&&) noexcept = default;

        private:
            void OnContactAdded(JPH::CharacterVirtual const* inCharacter,
                JPH::BodyID const& inBodyID2,
                JPH::SubShapeID const& inSubShapeID2,
                JPH::RVec3Arg inContactPosition,
                JPH::Vec3Arg inContactNormal,
                JPH::CharacterContactSettings& ioSettings) override;

        private:
            physics_engine_t* physics_engine_;
        };

    private:
        physics_engine_t* physics_engine_;
        ngnwsi::mouse_t* mouse_;

        glm::vec3 acceleration_{};

        contact_listener_t listener_;
        JPH::Ref<JPH::CharacterVirtual> physics_entity_;
    };
} // namespace galileo

#endif
