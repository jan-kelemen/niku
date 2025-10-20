#include <world_contact_listener.hpp>

#include <scripting.hpp>

#include <ngnscr_scripting_engine.hpp>

#include <angelscript.h>

#include <Jolt/Physics/Body/Body.h>

#include <spdlog/spdlog.h>

#include <cassert>
#include <cstdint>

// IWYU pragma: no_include <fmt/base.h>
// IWYU pragma: no_include <fmt/format.h>
// IWYU pragma: no_include <memory>

galileo::world_contact_listener_t::world_contact_listener_t(
    ngnscr::scripting_engine_t& scripting_engine,
    entt::registry& registry)
    : scripting_engine_{&scripting_engine}
    , registry_{&registry}
{
}

void galileo::world_contact_listener_t::OnContactPersisted(
    JPH::Body const& inBody1,
    JPH::Body const& inBody2,
    [[maybe_unused]] JPH::ContactManifold const& inManifold,
    [[maybe_unused]] JPH::ContactSettings& ioSettings)
{
    auto call_on_hit_script =
        [this](JPH::Body const& body, JPH::Body const& other)
    {
        auto* const scripts{registry_->try_get<component::scripts_t>(
            static_cast<entt::entity>(body.GetUserData()))};

        if (scripts && scripts->object && scripts->on_hit_script)
        {
            auto context{
                scripting_engine_->execution_context(scripts->on_hit_script)};
            assert(context);

            context->SetObject(scripts->object.get());

            context->SetArgDWord(0, static_cast<uint32_t>(other.GetUserData()));

            if (auto const execution_result{context->Execute()};
                execution_result != asEXECUTION_FINISHED)
            {
                if (execution_result == asEXECUTION_EXCEPTION)
                {
                    spdlog::error(
                        "An exception '{}' occurred. Please correct the code and try again.",
                        context->GetExceptionString());
                }
            }
        }
    };

    call_on_hit_script(inBody1, inBody2);
    call_on_hit_script(inBody2, inBody1);
}
