#ifndef NGNWSI_SDL_GUARD_INCLUDED

#include <cstdint>

namespace ngnwsi
{
    class sdl_window_t;
} // namespace ngnwsi

namespace ngnwsi
{
    class [[nodiscard]] sdl_guard_t final
    {
    public: // Construction
        explicit sdl_guard_t(uint32_t flags);

        sdl_guard_t(sdl_guard_t const&) = delete;

        sdl_guard_t(sdl_guard_t&&) noexcept = delete;

    public: // Destruction
        ~sdl_guard_t();

    public: // Operators
        sdl_guard_t& operator=(sdl_guard_t const&) = delete;

        sdl_guard_t& operator=(sdl_guard_t&&) = delete;
    };

    class [[nodiscard]] sdl_text_input_guard_t final
    {
    public:
        explicit sdl_text_input_guard_t(sdl_window_t const& window);

        sdl_text_input_guard_t(sdl_text_input_guard_t const&) = delete;

        sdl_text_input_guard_t(sdl_text_input_guard_t&&) noexcept = delete;

    public:
        ~sdl_text_input_guard_t();

    public:
        sdl_text_input_guard_t& operator=(
            sdl_text_input_guard_t const&) = delete;

        sdl_text_input_guard_t& operator=(
            sdl_text_input_guard_t&&) noexcept = delete;

    private:
        sdl_window_t const* window_;
    };
} // namespace ngnwsi

#endif // !NGNWSI_SDL_GUARD_INCLUDED
