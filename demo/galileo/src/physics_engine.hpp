#ifndef GALILEO_PHYSICS_ENGINE_INCLUDED
#define GALILEO_PHYSICS_ENGINE_INCLUDED

#include <ngnphy_jolt.hpp> // IWYU pragma: keep

#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

#include <memory>

namespace JPH
{
    class TempAllocator;
    class BodyInterface;
    class PhysicsSystem;
} // namespace JPH

namespace cppext
{
    class thread_pool_t;
} // namespace cppext

namespace galileo
{
    namespace object_layers
    {
        constexpr JPH::ObjectLayer moving{0};
        constexpr JPH::ObjectLayer non_moving{1};
        constexpr JPH::ObjectLayer count{2};
    } // namespace object_layers

    class [[nodiscard]] physics_engine_t final
    {
    public:
        explicit physics_engine_t(cppext::thread_pool_t& thread_pool);

        physics_engine_t(physics_engine_t const&) = delete;

        physics_engine_t(physics_engine_t&&) noexcept = default;

    public:
        ~physics_engine_t();

    public:
        void update(float delta_time);

        [[nodiscard]] JPH::PhysicsSystem& physics_system();

        [[nodiscard]] JPH::PhysicsSystem const& physics_system() const;

        [[nodiscard]] JPH::BodyInterface& body_interface();

        [[nodiscard]] JPH::BodyInterface const& body_interface() const;

        [[nodiscard]] JPH::TempAllocator& allocator();

    public:
        physics_engine_t& operator=(physics_engine_t const&) = delete;

        physics_engine_t& operator=(physics_engine_t&&) noexcept = delete;

    private:
        struct impl;
        std::unique_ptr<impl> impl_;
    };
} // namespace galileo

namespace galileo::component
{
    struct [[nodiscard]] physics_t final
    {
        JPH::BodyID id;
    };
} // namespace galileo::component

#endif
