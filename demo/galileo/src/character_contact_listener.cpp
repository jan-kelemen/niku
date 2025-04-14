#include <character_contact_listener.hpp>

#include <physics_engine.hpp>
#include <scripting.hpp>

#include <ngnscr_scripting_engine.hpp>

#include <angelscript.h>

#include <Jolt/Math/Vec3.h>
#include <Jolt/Physics/Body/BodyInterface.h>

#include <spdlog/spdlog.h>

#include <cassert>
#include <chrono>

// IWYU pragma: no_include <fmt/base.h>
// IWYU pragma: no_include <fmt/format.h>
// IWYU pragma: no_include <compare>

galileo::character_contact_listener_t::character_contact_listener_t(
    physics_engine_t& physics_engine,
    ngnscr::scripting_engine_t& scripting_engine,
    entt::registry& registry)
    : physics_engine_{&physics_engine}
    , scripting_engine_{&scripting_engine}
    , registry_{&registry}
{
}

void galileo::character_contact_listener_t::OnContactAdded(
    JPH::CharacterVirtual const* inCharacter,
    JPH::BodyID const& inBodyID2,
    [[maybe_unused]] JPH::SubShapeID const& inSubShapeID2,
    JPH::RVec3Arg inContactPosition,
    JPH::Vec3Arg inContactNormal,
    [[maybe_unused]] JPH::CharacterContactSettings& ioSettings)
{
    using namespace std::chrono_literals;

    auto& interface{physics_engine_->body_interface()};

    if (interface.GetObjectLayer(inBodyID2) == object_layers::non_moving)
    {
        auto* const scripts{registry_->try_get<component::scripts_t>(
            static_cast<entt::entity>(interface.GetUserData(inBodyID2)))};

        if (scripts && scripts->object && scripts->on_character_hit_script)
        {
            auto context{scripting_engine_->execution_context(
                scripts->on_character_hit_script)};
            assert(context);

            context->SetObject(scripts->object.get());

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
    }
    else
    {
        interface.AddForce(inBodyID2,
            inContactNormal * inCharacter->GetMass() * 100,
            inContactPosition);
    }
}
