#include <physics_engine.hpp>

#include <cppext_pragma_warning.hpp>

#include <ngnphy_jolt_job_system.hpp>

#include <Jolt/ConfigurationString.h>
#include <Jolt/Core/Core.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/IssueReporting.h>
#include <Jolt/Core/JobSystem.h>
#include <Jolt/Core/Memory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <spdlog/spdlog.h>

#include <array>
#include <cstdarg>
#include <cstdio>
#include <memory>
#include <string_view>

namespace JPH
{
    class BodyInterface;
} // namespace JPH

// IWYU pragma: no_include <fmt/base.h>
// IWYU pragma: no_include <fmt/format.h>
// IWYU pragma: no_include <Jolt/Core/STLAllocator.h>

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
        std::array<char, 1024> buffer{};
        DISABLE_WARNING_PUSH
        DISABLE_WARNING_FORMAT_NONLITERAL
        int const count{
            vsnprintf(buffer.data(), buffer.size(), format_string, list)};
        DISABLE_WARNING_POP
        va_end(list);
        // NOLINTEND

        if (count < 0)
        {
            return;
        }

        spdlog::info(
            std::string_view{buffer.data(), static_cast<size_t>(count)});
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

class [[nodiscard]] object_layer_pair_filter final
    : public JPH::ObjectLayerPairFilter
{
    [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer first,
        JPH::ObjectLayer second) const override
    {
        switch (first)
        {
        case galileo::object_layers::non_moving:
            // Non moving only collides with moving
            return second == galileo::object_layers::moving;
        case galileo::object_layers::moving:
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
        object_to_broad_mapping_[galileo::object_layers::non_moving] =
            broad_phase_layers::non_moving;
        object_to_broad_mapping_[galileo::object_layers::moving] =
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
        JPH_ASSERT(object_layer < galileo::object_layers::count);
        return object_to_broad_mapping_[object_layer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    [[nodiscard]] char const* GetBroadPhaseLayerName(
        JPH::BroadPhaseLayer layer) const override
    {
        switch ((JPH::BroadPhaseLayer::Type) layer)
        {
        case (JPH::BroadPhaseLayer::Type) galileo::object_layers::non_moving:
            return "non_moving";
        case (JPH::BroadPhaseLayer::Type) galileo::object_layers::moving:
            return "moving";
        default:
            JPH_ASSERT(false);
            return "invalid";
        }
    }
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

private:
    // NOLINTNEXTLINE
    JPH::BroadPhaseLayer
        object_to_broad_mapping_[galileo::object_layers::count];
};

class [[nodiscard]] object_vs_broad_phase_layer_filter final
    : public JPH::ObjectVsBroadPhaseLayerFilter
{
    [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer first,
        JPH::BroadPhaseLayer second) const override
    {
        switch (first)
        {
        case galileo::object_layers::non_moving:
            return second == broad_phase_layers::moving;
        case galileo::object_layers::moving:
            return true;
        default:
            JPH_ASSERT(false);
            return false;
        }
    }
};

struct [[nodiscard]] galileo::physics_engine_t::impl final
{
public:
    impl(cppext::thread_pool_t& thread_pool);

    impl(impl const&) = delete;

    impl(impl&&) noexcept = delete;

public:
    ~impl();

public:
    void fixed_update(float delta_time);

public:
    impl& operator=(impl const&) = delete;

    impl& operator=(impl&&) noexcept = delete;

public:
    std::unique_ptr<JPH::Factory> factory_;
    std::unique_ptr<JPH::TempAllocator> temp_allocator_;
    std::unique_ptr<JPH::JobSystem> job_system_;

    broad_phase_layer broad_phase_layer_;
    object_vs_broad_phase_layer_filter object_vs_broad_phase_layer_filter_;
    object_layer_pair_filter object_vs_object_layer_filter_;
    std::unique_ptr<JPH::PhysicsSystem> physics_system_;
};

galileo::physics_engine_t::impl::impl(cppext::thread_pool_t& thread_pool)
{
    JPH::RegisterDefaultAllocator();

    JPH::Trace = trace;
    // cppcheck-suppress unknownMacro
    JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = assert_fail;)

    factory_ = std::make_unique<JPH::Factory>();
    JPH::Factory::sInstance = factory_.get();

    JPH::RegisterTypes();

    spdlog::info("{}", JPH::GetConfigurationString());

    temp_allocator_ =
#ifdef JPH_DISABLE_TEMP_ALLOCATOR
        std::make_unique<JPH::TempAllocatorMalloc>();
#else
        std::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024);
#endif

    job_system_ = std::make_unique<ngnphy::job_system_t>(thread_pool);

    physics_system_ = std ::make_unique<JPH::PhysicsSystem>();
    physics_system_->Init(max_bodies,
        body_mutexes,
        max_body_pairs,
        max_contact_constraints,
        broad_phase_layer_,
        object_vs_broad_phase_layer_filter_,
        object_vs_object_layer_filter_);
    physics_system_->SetGravity(JPH::Vec3{0.0f, -9.81f, 0.0f});
}

galileo::physics_engine_t::impl::~impl()
{
    JPH::UnregisterTypes();

    JPH::Factory::sInstance = nullptr;
    factory_.reset();
}

// NOLINTNEXTLINE(readability-make-member-function-const)
void galileo::physics_engine_t::impl::fixed_update(float const delta_time)
{
    constexpr auto collision_steps{1};
    physics_system_->Update(delta_time,
        collision_steps,
        temp_allocator_.get(),
        job_system_.get());
}

galileo::physics_engine_t::physics_engine_t(cppext::thread_pool_t& thread_pool)
    : impl_{std::make_unique<impl>(thread_pool)}
{
}

galileo::physics_engine_t::~physics_engine_t() = default;

void galileo::physics_engine_t::update(float const delta_time)
{
    impl_->fixed_update(delta_time);
}

JPH::PhysicsSystem& galileo::physics_engine_t::physics_system()
{
    return *impl_->physics_system_;
}

JPH::PhysicsSystem const& galileo::physics_engine_t::physics_system() const
{
    return *impl_->physics_system_;
}

JPH::BodyInterface& galileo::physics_engine_t::body_interface()
{
    return impl_->physics_system_->GetBodyInterface();
}

JPH::BodyInterface const& galileo::physics_engine_t::body_interface() const
{
    return impl_->physics_system_->GetBodyInterface();
}

JPH::TempAllocator& galileo::physics_engine_t::allocator()
{
    return *impl_->temp_allocator_;
}
