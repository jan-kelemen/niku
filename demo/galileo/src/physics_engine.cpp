#include <physics_engine.hpp>

#include <cppext_pragma_warning.hpp>

#include <Jolt/Jolt.h> // IWYU pragma: keep

#include <Jolt/ConfigurationString.h>
#include <Jolt/Core/Array.h>
#include <Jolt/Core/Core.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/IssueReporting.h>
#include <Jolt/Core/JobSystem.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/Memory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/MotionType.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/EActivation.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <spdlog/spdlog.h>

#include <array>
#include <cstdarg>
#include <cstdio>
#include <memory>
#include <string_view>
#include <thread>

// IWYU pragma: no_include <fmt/base.h>
// IWYU pragma: no_include <fmt/format.h>
// IWYU pragma: no_include <Jolt/Core/STLAllocator.h>
// IWYU pragma: no_include <Jolt/Math/Real.h>

namespace
{
    // Max amount of rigid bodies in the physics system
    constexpr unsigned int max_bodies{1024};

    // Amount of mutexes for rigid body concurrency
    constexpr unsigned int body_mutexes{0};

    // Max amount of body pairs for broad phace collision
    constexpr unsigned int max_body_pairs{1024};

    // Contact constraint buffer
    constexpr unsigned int max_contact_constraints{1024};

    void trace(char const* const format_string, ...)
    {
        // NOLINTBEGIN
        va_list list;
        va_start(list, format_string);
        std::array<char, 1024> buffer;
        DISABLE_WARNING_PUSH
        DISABLE_WARNING_FORMAT_NONLITERAL
        vsnprintf(buffer.data(), buffer.size(), format_string, list);
        DISABLE_WARNING_POP
        va_end(list);
        // NOLINTEND

        spdlog::info(std::string_view{buffer.data(), buffer.size()});
    }

    DISABLE_WARNING_PUSH

    DISABLE_WARNING_UNUSED_FUNCTION
    bool assert_fail(char const* const expression,
        char const* const message,
        char const* const file,
        unsigned int const line)
    {
        spdlog::error("{}:{}: ({}) {}",
            file,
            line,
            expression,
            message ? "" : message);

#ifdef NDEBUG
        return false;
#else
        return true;
#endif
    }

    DISABLE_WARNING_POP
} // namespace

namespace object_layers
{
    constexpr JPH::ObjectLayer moving{0};
    constexpr JPH::ObjectLayer non_moving{1};
    constexpr JPH::ObjectLayer count{2};
} // namespace object_layers

class [[nodiscard]] object_layer_pair_filter final
    : public JPH::ObjectLayerPairFilter
{
    [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer first,
        JPH::ObjectLayer second) const override
    {
        switch (first)
        {
        case object_layers::non_moving:
            // Non moving only collides with moving
            return second == object_layers::moving;
        case object_layers::moving:
            // Moving collides with everything
            return true;
        default:
            JPH_ASSERT(false);
            return false;
        }
    }
};

namespace broad_phase_layers
{
    constexpr JPH::BroadPhaseLayer non_moving{0};
    constexpr JPH::BroadPhaseLayer moving{1};
    constexpr unsigned int count{2};
} // namespace broad_phase_layers

class [[nodiscard]] broad_phase_layer final
    : public JPH::BroadPhaseLayerInterface
{
public:
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
    broad_phase_layer()
    {
        // Create a mapping table from object to broad phase layer
        object_to_broad_mapping_[object_layers::non_moving] =
            broad_phase_layers::non_moving;
        object_to_broad_mapping_[object_layers::moving] =
            broad_phase_layers::moving;
    }

    broad_phase_layer(broad_phase_layer const&) = delete;

    broad_phase_layer(broad_phase_layer&&) noexcept = delete;

public:
    ~broad_phase_layer() override = default;

public:
    broad_phase_layer& operator=(broad_phase_layer const&) = delete;

    broad_phase_layer& operator=(broad_phase_layer&&) noexcept = delete;

public:
    [[nodiscard]] JPH::uint GetNumBroadPhaseLayers() const override
    {
        return broad_phase_layers::count;
    }

    [[nodiscard]] JPH::BroadPhaseLayer GetBroadPhaseLayer(
        JPH::ObjectLayer object_layer) const override
    {
        JPH_ASSERT(object_layer < object_layers::count);
        return object_to_broad_mapping_[object_layer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    [[nodiscard]] char const* GetBroadPhaseLayerName(
        JPH::BroadPhaseLayer layer) const override
    {
        switch ((JPH::BroadPhaseLayer::Type) layer)
        {
        case (JPH::BroadPhaseLayer::Type) object_layers::non_moving:
            return "non_moving";
        case (JPH::BroadPhaseLayer::Type) object_layers::moving:
            return "moving";
        default:
            JPH_ASSERT(false);
            return "invalid";
        }
    }
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

private:
    // NOLINTNEXTLINE
    JPH::BroadPhaseLayer object_to_broad_mapping_[object_layers::count];
};

class [[nodiscard]] object_vs_broad_phase_layer_filter final
    : public JPH::ObjectVsBroadPhaseLayerFilter
{
    [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer first,
        JPH::BroadPhaseLayer second) const override
    {
        switch (first)
        {
        case object_layers::non_moving:
            return second == broad_phase_layers::moving;
        case object_layers::moving:
            return true;
        default:
            JPH_ASSERT(false);
            return false;
        }
    }
};

class [[nodiscard]] contact_listener final : public JPH::ContactListener
{
    JPH::ValidateResult OnContactValidate(
        [[maybe_unused]] JPH::Body const& first,
        [[maybe_unused]] JPH::Body const& second,
        [[maybe_unused]] JPH::RVec3Arg base_offset,
        [[maybe_unused]] JPH::CollideShapeResult const& collision_result)
        override
    {
        spdlog::info("Contact validate callback");

        // Allows you to ignore a contact before it is created (using layers to
        // not make objects collide is cheaper!)
        return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
    }

    void OnContactAdded([[maybe_unused]] JPH::Body const& first,
        [[maybe_unused]] JPH::Body const& second,
        [[maybe_unused]] JPH::ContactManifold const& manifold,
        [[maybe_unused]] JPH::ContactSettings& settings) override
    {
        spdlog::info("A contact was added");
    }

    void OnContactPersisted([[maybe_unused]] JPH::Body const& first,
        [[maybe_unused]] JPH::Body const& second,
        [[maybe_unused]] JPH::ContactManifold const& manifold,
        [[maybe_unused]] JPH::ContactSettings& settings) override
    {
        spdlog::info("A contact was persisted");
    }

    void OnContactRemoved(
        [[maybe_unused]] JPH::SubShapeIDPair const& subshape_id_pair) override
    {
        spdlog::info("A contact was removed");
    }
};

class [[nodiscard]] body_activation_listener final
    : public JPH::BodyActivationListener
{
    void OnBodyActivated([[maybe_unused]] JPH::BodyID const& body,
        [[maybe_unused]] JPH::uint64 body_user_data) override
    {
        spdlog::info("A body got activated");
    }

    void OnBodyDeactivated([[maybe_unused]] JPH::BodyID const& body,
        [[maybe_unused]] JPH::uint64 body_user_data) override
    {
        spdlog::info("A body went to sleep");
    }
};

namespace // TODO-JK: Temporary code
{
    void setup_world(JPH::BodyInterface& body_interface)
    {
        using namespace JPH::literals;

        // Next we can create a rigid body to serve as the floor, we make a
        // large box Create the settings for the collision volume (the shape).
        // Note that for simple shapes (like boxes) you can also directly
        // construct a BoxShape.
        JPH::BoxShapeSettings const floor_shape_settings{
            JPH::Vec3{100.0f, 1.0f, 100.0f}};
        floor_shape_settings
            .SetEmbedded(); // A ref counted object on the stack (base class
                            // RefTarget) should be marked as such to prevent it
                            // from being freed when its reference count goes to
                            // 0.

        // Create the shape
        JPH::ShapeSettings::ShapeResult const floor_shape_result{
            floor_shape_settings.Create()};
        JPH::ShapeRefC const floor_shape{floor_shape_result
                .Get()}; // We don't expect an error here, but you can check
                         // floor_shape_result for HasError() / GetError()

        // Create the settings for the body itself. Note that here you can also
        // set other properties like the restitution / friction.
        JPH::BodyCreationSettings const floor_settings{floor_shape,
            JPH::RVec3{0.0_r, -1.0_r, 0.0_r},
            JPH::Quat::sIdentity(),
            JPH::EMotionType::Static,
            object_layers::non_moving};

        // Create the actual rigid body
        JPH::Body* floor{body_interface.CreateBody(
            floor_settings)}; // Note that if we run out of bodies this can
                              // return nullptr

        // Add it to the world
        body_interface.AddBody(floor->GetID(), JPH::EActivation::DontActivate);

        // Now create a dynamic body to bounce on the floor
        // Note that this uses the shorthand version of creating and adding a
        // body to the world
        JPH::BodyCreationSettings const sphere_settings{
            new JPH::SphereShape{0.5f},
            JPH::RVec3{0.0_r, 2.0_r, 0.0_r},
            JPH::Quat::sIdentity(),
            JPH::EMotionType::Dynamic,
            object_layers::moving};
        JPH::BodyID const sphere_id =
            body_interface.CreateAndAddBody(sphere_settings,
                JPH::EActivation::Activate);

        // Now you can interact with the dynamic body, in this case we're going
        // to give it a velocity. (note that if we had used CreateBody then we
        // could have set the velocity straight on the body before adding it to
        // the physics system)
        body_interface.SetLinearVelocity(sphere_id,
            JPH::Vec3{0.0f, -5.0f, 0.0f});
    }
} // namespace

class [[nodiscard]] galileo::physics_engine_t::impl final
{
public:
    impl();

    impl(impl const&) = delete;

    impl(impl&&) noexcept = delete;

public:
    ~impl();

public:
    void fixed_update(float delta_time);

public:
    impl& operator=(impl const&) = delete;

    impl& operator=(impl&&) noexcept = delete;

private:
    std::unique_ptr<JPH::Factory> factory_;
    std::unique_ptr<JPH::TempAllocator> temp_allocator_;
    std::unique_ptr<JPH::JobSystem> job_system_;

    broad_phase_layer broad_phase_layer_;
    object_vs_broad_phase_layer_filter object_vs_broad_phase_layer_filter_;
    object_layer_pair_filter object_vs_object_layer_filter_;
    body_activation_listener body_activation_listener_;
    contact_listener contact_listener_;
    std::unique_ptr<JPH::PhysicsSystem> physics_system_;
};

galileo::physics_engine_t::impl::impl()
{
    JPH::RegisterDefaultAllocator();

    JPH::Trace = trace;
    // cppcheck-suppress unknownMacro
    JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = assert_fail;)

    factory_ = std::make_unique<JPH::Factory>();
    JPH::Factory::sInstance = factory_.get();

    JPH::RegisterTypes();

    spdlog::error("{}", JPH::GetConfigurationString());

    temp_allocator_ =
#ifdef JPH_DISABLE_TEMP_ALLOCATOR
        std::make_unique<JPH::TempAllocatorMalloc>();
#else
        std::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024);
#endif

    job_system_ =
        std::make_unique<JPH::JobSystemThreadPool>(JPH::cMaxPhysicsJobs,
            JPH::cMaxPhysicsBarriers,
            std::thread::hardware_concurrency() - 1);

    physics_system_ = std ::make_unique<JPH::PhysicsSystem>();
    physics_system_->Init(max_bodies,
        body_mutexes,
        max_body_pairs,
        max_contact_constraints,
        broad_phase_layer_,
        object_vs_broad_phase_layer_filter_,
        object_vs_object_layer_filter_);
    physics_system_->SetBodyActivationListener(&body_activation_listener_);
    physics_system_->SetContactListener(&contact_listener_);

    auto& body_interface{physics_system_->GetBodyInterface()};
    setup_world(body_interface);
}

galileo::physics_engine_t::impl::~impl()
{
    JPH::UnregisterTypes();

    JPH::Factory::sInstance = nullptr;
    factory_.reset();
}

void galileo::physics_engine_t::impl::fixed_update(float delta_time)
{
    JPH::Array<JPH::BodyID> bodies;
    physics_system_->GetBodies(bodies);

    auto& body_interface{physics_system_->GetBodyInterface()};
    for (auto const& body_id : bodies)
    {
        JPH::RVec3 const position{
            body_interface.GetCenterOfMassPosition(body_id)};
        JPH::Vec3 const velocity{body_interface.GetLinearVelocity(body_id)};
        spdlog::info("Body {}: Position = ({}, {}, {}), Velocity =({}, {}, {})",
            body_id.GetIndexAndSequenceNumber(),
            position.GetX(),
            position.GetY(),
            position.GetZ(),
            velocity.GetX(),
            velocity.GetY(),
            velocity.GetZ());
    }

    constexpr auto collision_steps{1};
    physics_system_->Update(delta_time,
        collision_steps,
        temp_allocator_.get(),
        job_system_.get());
}

galileo::physics_engine_t::physics_engine_t() : impl_{std::make_unique<impl>()}
{
}

galileo::physics_engine_t::~physics_engine_t() = default;

void galileo::physics_engine_t::fixed_update(float const delta_time)
{
    impl_->fixed_update(delta_time);
}
