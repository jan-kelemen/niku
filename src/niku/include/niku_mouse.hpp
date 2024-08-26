#ifndef NIKU_MOUSE_INCLUDED
#define NIKU_MOUSE_INCLUDED

#include <glm/vec2.hpp>

namespace niku
{
    class [[nodiscard]] mouse_t
    {
    public:
        mouse_t();

        explicit mouse_t(bool capture);

        mouse_t(mouse_t const&) = delete;

        mouse_t(mouse_t&&) noexcept = delete;

    public:
        ~mouse_t() = default;

    public:
        [[nodiscard]] glm::ivec2 position() const;

        [[nodiscard]] glm::ivec2 relative_offset() const;

        [[nodiscard]] bool captured() const;

        void set_capture(bool value);

    public:
        mouse_t& operator=(mouse_t const&) = delete;

        mouse_t& operator=(mouse_t&&) noexcept = delete;

    private:
        bool captured_;
    };
} // namespace niku
#endif
