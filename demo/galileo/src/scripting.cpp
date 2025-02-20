#include <scripting.hpp>

#include <ngnscr_scripting_engine.hpp>

#include <angelscript.h>

bool galileo::register_spawner_type(
    ngnscr::scripting_engine_t& scripting_engine)
{
    if (scripting_engine.engine().RegisterObjectType("spawner_t",
            sizeof(spawner_t),
            asOBJ_REF | asOBJ_NOCOUNT) < 0)
    {
        return false;
    }

    if (scripting_engine.engine().RegisterObjectMethod("spawner_t",
            "bool should_spawn_sphere()",
            asMETHOD(spawner_t, should_spawn_sphere),
            asCALL_THISCALL) < 0)
    {
        return false;
    }

    return true;
}

galileo::spawner_t::~spawner_t()
{
}

bool galileo::spawner_t::should_spawn_sphere()
{
    using namespace std::chrono_literals;

    auto const now{std::chrono::steady_clock::now()};
    if (auto const diff{now - last_spawn_}; diff < 5s)
    {
        return false;
    }
    last_spawn_ = now;

    return true;
}
