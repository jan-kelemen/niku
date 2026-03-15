#ifndef NGNWSI_FIXED_TIMESTEP_INCLUDED
#define NGNWSI_FIXED_TIMESTEP_INCLUDED

#include <cstdint>

namespace ngnwsi
{
    class [[nodiscard]] fixed_timestep_t final
    {
    public:
        explicit fixed_timestep_t(float ups = 60.0f);

    public:
        float update_interval{};

    public:
        [[nodiscard]] uint64_t pending_simulation_steps();

    private:
        float frequency_{};
        float accumulated_error_{};
        uint64_t last_tick_;
    };
} // namespace ngnwsi

#endif
