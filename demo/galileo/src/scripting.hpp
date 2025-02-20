#ifndef GALILEO_SCRIPTING_INCLUDED
#define GALILEO_SCRIPTING_INCLUDED

#include <chrono>

namespace ngnscr
{
    class scripting_engine_t;
} // namespace ngnscr

class asIScriptFunction;
class asIScriptObject;
class asITypeInfo;

namespace galileo
{
    [[nodiscard]] bool register_spawner_type(
        ngnscr::scripting_engine_t& scripting_engine);

    class [[nodiscard]] spawner_t final
    {
    public:
        static constexpr auto in_place_delete = true;

    public:
        spawner_t() = default;

        spawner_t(spawner_t const&) = delete;

        spawner_t(spawner_t&&) noexcept = delete;

    public:
        ~spawner_t();

    public:
        spawner_t& operator=(spawner_t const&) = delete;

        spawner_t& operator=(spawner_t&&) = delete;

    private:
        bool should_spawn_sphere();

    private:
        std::chrono::steady_clock::time_point last_spawn_{
            std::chrono::steady_clock::now()};

        friend bool register_spawner_type(
            ngnscr::scripting_engine_t& scripting_engine);
    };
} // namespace galileo

namespace galileo::component
{
    struct [[nodiscard]] scripts_t final
    {
        asITypeInfo* type{nullptr};
        asIScriptFunction* factory{nullptr};
        asIScriptObject* object{nullptr};
        asIScriptFunction* on_character_hit_script{nullptr};
    };
} // namespace galileo::component

#endif
