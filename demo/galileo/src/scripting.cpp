#include <scripting.hpp>

#include <ngnscr_scripting_engine.hpp>
#include <ngnscr_types.hpp>

#include <angelscript.h>

#include <spdlog/spdlog.h>

#include <cassert>
#include <cstdint>
#include <expected>

// IWYU pragma: no_include <fmt/base.h>
// IWYU pragma: no_include <fmt/format.h>
// IWYU pragma: no_include <compare>
// IWYU pragma: no_include <memory>
// IWYU pragma: no_include <ratio>

bool galileo::register_spawner_type(
    ngnscr::scripting_engine_t& scripting_engine)
{
    if (scripting_engine.engine().RegisterObjectType("spawner_data",
            sizeof(spawner_data_t),
            asOBJ_REF | asOBJ_NOCOUNT) < 0)
    {
        return false;
    }

    if (scripting_engine.engine().RegisterObjectMethod("spawner_data",
            "bool should_spawn_sphere()",
            asMETHOD(spawner_data_t, should_spawn_sphere),
            asCALL_THISCALL) < 0)
    {
        return false;
    }

    return true;
}

bool galileo::spawner_data_t::should_spawn_sphere()
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

std::expected<galileo::component::scripts_t, asEContextState>
galileo::component::create_spawner_scripts(spawner_data_t& spawner,
    ngnscr::scripting_engine_t& engine,
    asIScriptModule const& module)
{
    scripts_t rv;
    asITypeInfo* const type{module.GetTypeInfoByName("spawner")};
    assert(type);

    rv.factory =
        module.GetFunctionByDecl("spawner@ create_spawner(spawner_data@)");
    assert(rv.factory);

    rv.on_character_hit_script =
        type->GetMethodByDecl("void on_character_contact()");
    assert(rv.on_character_hit_script);

    auto context{engine.execution_context(rv.factory)};
    assert(context);

    context->SetArgObject(0, &spawner);

    if (auto const execution_result{context->Execute()};
        execution_result != asEXECUTION_FINISHED)
    {
        if (execution_result == asEXECUTION_EXCEPTION)
        {
            spdlog::error(
                "An exception '{}' occurred. Please correct the code and try again.",
                context->GetExceptionString());
        }

        return std::unexpected{static_cast<asEContextState>(execution_result)};
    }
    rv.object =
        *static_cast<asIScriptObject**>(context->GetAddressOfReturnValue());
    assert(rv.object);

    return rv;
}

std::expected<galileo::component::scripts_t, asEContextState>
galileo::component::create_sphere_scripts(entt::entity sphere,
    ngnscr::scripting_engine_t& engine,
    asIScriptModule const& module)
{
    scripts_t rv;
    asITypeInfo* const type{module.GetTypeInfoByName("sphere")};
    assert(type);

    rv.factory = module.GetFunctionByDecl("sphere@ create_sphere(uint32)");
    assert(rv.factory);

    rv.on_hit_script = type->GetMethodByDecl("void on_hit(uint32)");
    assert(rv.on_hit_script);

    auto context{engine.execution_context(rv.factory)};
    assert(context);

    context->SetArgDWord(0, static_cast<uint32_t>(sphere));

    if (auto const execution_result{context->Execute()};
        execution_result != asEXECUTION_FINISHED)
    {
        if (execution_result == asEXECUTION_EXCEPTION)
        {
            spdlog::error(
                "An exception '{}' occurred. Please correct the code and try again.",
                context->GetExceptionString());
        }

        return std::unexpected{static_cast<asEContextState>(execution_result)};
    }
    rv.object =
        *static_cast<asIScriptObject**>(context->GetAddressOfReturnValue());
    assert(rv.object);

    return rv;
}
