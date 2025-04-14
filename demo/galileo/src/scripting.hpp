#ifndef GALILEO_SCRIPTING_INCLUDED
#define GALILEO_SCRIPTING_INCLUDED

#include <ngnscr_types.hpp>

#include <angelscript.h>

#include <entt/entt.hpp> // IWYU pragma: keep

#include <chrono>
#include <expected>

namespace ngnscr
{
    class scripting_engine_t;
} // namespace ngnscr

namespace galileo
{
    [[nodiscard]] bool register_spawner_type(
        ngnscr::scripting_engine_t& scripting_engine);

    class [[nodiscard]] spawner_data_t final
    {
    public:
        static constexpr auto in_place_delete = true;

    public:
        spawner_data_t() = default;

        spawner_data_t(spawner_data_t const&) = delete;

        spawner_data_t(spawner_data_t&&) noexcept = delete;

    public:
        ~spawner_data_t() = default;

    public:
        spawner_data_t& operator=(spawner_data_t const&) = delete;

        spawner_data_t& operator=(spawner_data_t&&) = delete;

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
        asIScriptFunction* factory{nullptr};
        asIScriptFunction* on_character_hit_script{nullptr};
        asIScriptFunction* on_hit_script{nullptr};

        ngnscr::script_object_ptr_t object{nullptr};
    };

    [[nodiscard]] std::expected<scripts_t, asEContextState>
    create_spawner_scripts(spawner_data_t& spawner,
        ngnscr::scripting_engine_t& engine,
        asIScriptModule const& module);

    [[nodiscard]] std::expected<scripts_t, asEContextState>
    create_sphere_scripts(entt::entity sphere,
        ngnscr::scripting_engine_t& engine,
        asIScriptModule const& module);
} // namespace galileo::component

#endif
