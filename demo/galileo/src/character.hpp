#ifndef GALILEO_CHARACTER_INCLUDED
#define GALILEO_CHARACTER_INCLUDED

#include <ngnphy_jolt.hpp> // IWYU pragma: keep

#include <Jolt/Core/Reference.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>

#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
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
    enum class [[nodiscard]] character_action_t
    {
        none,
        select_body,
    };

    class [[nodiscard]] character_t final
    {
    public:
        static constexpr float max_slope_angle{glm::radians(45.0f)};

    public:
        explicit character_t(physics_engine_t& physics_engine,
            ngnwsi::mouse_t& mouse);

    public:
        [[nodiscard]] character_action_t handle_event(SDL_Event const& event,
            float delta_time);

        void set_contact_listener(JPH::CharacterContactListener* listener);

        void update(float delta_time);

        void debug(physics_debug_t* physics_debug);

        [[nodiscard]] glm::vec3 position() const;

        void set_position(glm::vec3 position);

        [[nodiscard]] glm::vec3 center_of_mass() const;

        [[nodiscard]] glm::quat rotation() const;

        [[nodiscard]] glm::mat4 world_transform() const;

    private:
        physics_engine_t* physics_engine_;
        ngnwsi::mouse_t* mouse_;

        JPH::Ref<JPH::CharacterVirtual> physics_entity_;
    };
} // namespace galileo

namespace galileo::component
{
    struct [[nodiscard]] character_t final
    {
    };
} // namespace galileo::component

#endif
