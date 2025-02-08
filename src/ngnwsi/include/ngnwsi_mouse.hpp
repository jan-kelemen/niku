#ifndef NGNWSI_MOUSE_INCLUDED
#define NGNWSI_MOUSE_INCLUDED

#include <glm/vec2.hpp>

namespace ngnwsi
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
        void set_window_handle(void* window);

        [[nodiscard]] glm::vec2 position() const;

        [[nodiscard]] glm::vec2 relative_offset() const;

        [[nodiscard]] bool captured() const;

        void set_capture(bool value);

    public:
        mouse_t& operator=(mouse_t const&) = delete;

        mouse_t& operator=(mouse_t&&) noexcept = delete;

    private:
        void* window_handle_{nullptr};
        bool captured_;
    };
} // namespace ngnwsi
#endif
