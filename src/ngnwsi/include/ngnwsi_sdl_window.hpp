#ifndef NGNWSI_SDL_WINDOW_INCLUDED
#define NGNWSI_SDL_WINDOW_INCLUDED

#include <vkrndr_window.hpp>

#include <SDL3/SDL_video.h>

#include <volk.h>

#include <cstdint>
#include <vector> // IWYU pragma: keep

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

    class [[nodiscard]] sdl_window_t final : public vkrndr::window_t
    {
    public: // Construction
        sdl_window_t(char const* title,
            SDL_WindowFlags window_flags,
            int width,
            int height);

        sdl_window_t(sdl_window_t const&) = delete;

        sdl_window_t(sdl_window_t&&) noexcept = delete;

    public: // Destruction
        ~sdl_window_t() override;

    public: // Interface
        [[nodiscard]] constexpr SDL_Window* native_handle() const noexcept;

    public: // window_t implementation
        [[nodiscard]] std::vector<char const*>
        required_extensions() const override;

        [[nodiscard]] VkSurfaceKHR create_surface(VkInstance instance) override;

        void destroy_surface(VkInstance instance) override;

        [[nodiscard]] VkSurfaceKHR surface() const override;

        [[nodiscard]] VkExtent2D swap_extent(
            VkSurfaceCapabilitiesKHR const& capabilities) const override;

        [[nodiscard]] bool is_minimized() const override;

    public: // Operators
        sdl_window_t& operator=(sdl_window_t const&) = delete;

        sdl_window_t& operator=(sdl_window_t&&) noexcept = delete;

    private: // Data
        SDL_Window* window_;
        VkSurfaceKHR surface_{VK_NULL_HANDLE};
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

[[nodiscard]]
constexpr SDL_Window* ngnwsi::sdl_window_t::native_handle() const noexcept
{
    return window_;
}

#endif // !VKRNDR_SDL_WINDOW_INCLUDED
