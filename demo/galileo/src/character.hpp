#ifndef GALILEO_CHARACTER_INCLUDED
#define GALILEO_CHARACTER_INCLUDED

#include <Jolt/Jolt.h> // IWYU pragma: keep

#include <Jolt/Core/Reference.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>

#include <glm/vec3.hpp>

union SDL_Event;

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
        explicit character_t(physics_engine_t& physics_engine);

    public:
        void update(float delta_time);

        void debug(physics_debug_t* physics_debug);

        void set_position(glm::vec3 position);

    private:
        physics_engine_t* physics_engine_;

        glm::vec3 acceleration_;

        JPH::Ref<JPH::CharacterVirtual> physics_entity_;
    };
} // namespace galileo

#endif
