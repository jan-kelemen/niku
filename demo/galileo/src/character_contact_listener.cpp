#include <character_contact_listener.hpp>

#include <physics_engine.hpp>

#include <ngnscr_scripting_engine.hpp>

#include <angelscript.h>

#include <Jolt/Jolt.h> // IWYU pragma: keep

#include <Jolt/Math/Vec3.h>
#include <Jolt/Physics/Body/BodyInterface.h>

#include <spdlog/spdlog.h>

#include <exception>
#include <memory>

// IWYU pragma: no_include <fmt/base.h>
// IWYU pragma: no_include <fmt/format.h>
// IWYU pragma: no_include <compare>

galileo::character_contact_listener_t::character_contact_listener_t(
    physics_engine_t& physics_engine,
    ngnscr::scripting_engine_t& scripting_engine)
    : physics_engine_{&physics_engine}
    , scripting_engine_{&scripting_engine}
{
    // Find the function that is to be called.
    asIScriptModule* const mod{
        scripting_engine_->engine().GetModule("MyModule")};
    script_ = mod->GetFunctionByDecl("void main()");

    if (!script_)
    {
        std::terminate();
    }
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

    if (interface.GetObjectLayer(inBodyID2) == object_layers::non_moving &&
        interface.GetUserData(inBodyID2) == 1)
    {
        auto const now{std::chrono::steady_clock::now()};
        if (auto const diff{now - last_spawn_}; diff < 5s)
        {
            return;
        }
        last_spawn_ = now;

        auto context{scripting_engine_->execution_context(script_)};
        if (!context)
        {
            std::terminate();
        }

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
    else
    {
        interface.AddForce(inBodyID2,
            inContactNormal * inCharacter->GetMass() * 100,
            inContactPosition);
    }
}
