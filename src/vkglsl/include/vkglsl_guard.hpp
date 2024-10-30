#ifndef VKGLSL_GUARD_INCLUDED
#define VKGLSL_GUARD_INCLUDED

namespace vkglsl
{
    struct [[nodiscard]] guard_t final
    {
    public:
        guard_t();

        guard_t(guard_t const&) = delete;

        guard_t(guard_t&&) noexcept = delete;

    public:
        ~guard_t();

    public:
        guard_t& operator=(guard_t const&) = delete;

        guard_t& operator=(guard_t&&) noexcept = delete;
    };
} // namespace vkglsl
#endif
