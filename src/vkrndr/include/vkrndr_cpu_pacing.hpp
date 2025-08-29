#ifndef VKRNDR_CPU_PACING_INCLUDED
#define VKRNDR_CPU_PACING_INCLUDED

#include <boost/container/deque.hpp>

#include <volk.h>

#include <cstdint>
#include <functional>

namespace vkrndr
{
    struct device_t;
} // namespace vkrndr

namespace vkrndr
{
    struct [[nodiscard]] frame_in_flight_t final
    {
        uint32_t index{};

        VkFence submit_fence;

        boost::container::deque<std::function<void()>> cleanup;
        boost::container::deque<std::function<void()>> old_cleanup_queue;
    };

    class [[nodiscard]] cpu_pacing_t final
    {
    public:
        cpu_pacing_t(device_t const& device, uint32_t frames_in_flight);

        cpu_pacing_t(cpu_pacing_t const&) = delete;

        cpu_pacing_t(cpu_pacing_t&&) noexcept = delete;

    public:
        ~cpu_pacing_t();

    public:
        [[nodiscard]] frame_in_flight_t& current() noexcept;

        [[nodiscard]] frame_in_flight_t const& current() const noexcept;

        [[nodiscard]] frame_in_flight_t* pace(uint64_t timeout = 0);

        void begin_frame();

        void end_frame();

    public:
        cpu_pacing_t& operator=(cpu_pacing_t const&) = delete;

        cpu_pacing_t& operator=(cpu_pacing_t&&) noexcept = delete;

    private:
        device_t const* device_;
        std::vector<frame_in_flight_t> frames_;
        uint32_t frames_in_flight_;
        uint32_t current_frame_{};
    };
} // namespace vkrndr

inline vkrndr::frame_in_flight_t& vkrndr::cpu_pacing_t::current() noexcept
{
    return frames_[current_frame_];
}

inline vkrndr::frame_in_flight_t const&
vkrndr::cpu_pacing_t::current() const noexcept
{
    return frames_[current_frame_];
}

#endif
