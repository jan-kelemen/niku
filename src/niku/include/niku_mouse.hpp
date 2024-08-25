#ifndef NIKU_MOUSE_INCLUDED
#define NIKU_MOUSE_INCLUDED

#include <glm/vec2.hpp>

namespace niku
{
    class [[nodiscard]] mouse
    {
    public:
        mouse();

        explicit mouse(bool capture);

        mouse(mouse const&) = delete;

        mouse(mouse&&) noexcept = delete;

    public:
        ~mouse() = default;

    public:
        [[nodiscard]] glm::ivec2 position() const;

        [[nodiscard]] glm::ivec2 relative_offset() const;

        [[nodiscard]] bool captured() const;

        void set_capture(bool value);

    public:
        mouse& operator=(mouse const&) = delete;

        mouse& operator=(mouse&&) noexcept = delete;

    private:
        bool captured_;
    };
} // namespace niku
#endif
