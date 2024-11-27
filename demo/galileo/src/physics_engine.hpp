#ifndef GALILEO_PHYSICS_ENGINE_INCLUDED
#define GALILEO_PHYSICS_ENGINE_INCLUDED

#include <memory>

namespace galileo
{
    class [[nodiscard]] physics_engine_t final
    {
    public:
        physics_engine_t();

        physics_engine_t(physics_engine_t const&) = delete;

        physics_engine_t(physics_engine_t&&) noexcept = default;

    public:
        ~physics_engine_t();

    public:
        void fixed_update(float delta_time);

    public:
        physics_engine_t& operator=(physics_engine_t const&) = delete;

        physics_engine_t& operator=(physics_engine_t&&) noexcept = delete;

    private:
        class impl;
        std::unique_ptr<impl> impl_;
    };
} // namespace galileo

#endif
