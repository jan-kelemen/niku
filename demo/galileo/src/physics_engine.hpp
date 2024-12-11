#ifndef GALILEO_PHYSICS_ENGINE_INCLUDED
#define GALILEO_PHYSICS_ENGINE_INCLUDED

#include <Jolt/Jolt.h> // IWYU pragma: keep

#include <Jolt/Physics/Collision/ObjectLayer.h>

#include <memory>

namespace JPH
{
    class BodyInterface;
    class PhysicsSystem;
} // namespace JPH

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
        physics_engine_t();

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

    public:
        physics_engine_t& operator=(physics_engine_t const&) = delete;

        physics_engine_t& operator=(physics_engine_t&&) noexcept = delete;

    private:
        struct impl;
        std::unique_ptr<impl> impl_;
    };
} // namespace galileo

#endif
