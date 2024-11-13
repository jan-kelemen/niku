#ifndef NIKU_SDL_WINDOW_INCLUDED
#define NIKU_SDL_WINDOW_INCLUDED

#include <vkrndr_window.hpp>

#include <SDL2/SDL_video.h>

#include <volk.h>

#include <cstdint>
#include <vector> // IWYU pragma: keep

namespace niku
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
            bool centered,
            int width,
            int height);

        sdl_window_t(sdl_window_t const&) = delete;

        sdl_window_t(sdl_window_t&&) noexcept = delete;

    public: // Destruction
        ~sdl_window_t() override;

    public: // Interface
        [[nodiscard]] constexpr SDL_Window* native_handle() const noexcept;

    public: // vulkan_window implementation
        [[nodiscard]] std::vector<char const*>
        required_extensions() const override;

        [[nodiscard]] VkResult create_surface(VkInstance instance,
            VkSurfaceKHR& surface) const override;

        [[nodiscard]] VkExtent2D swap_extent(
            VkSurfaceCapabilitiesKHR const& capabilities) const override;

        [[nodiscard]] bool is_minimized() const override;

    public: // Operators
        sdl_window_t& operator=(sdl_window_t const&) = delete;

        sdl_window_t& operator=(sdl_window_t&&) noexcept = delete;

    private: // Data
        SDL_Window* window_;
    };
} // namespace niku

[[nodiscard]]
constexpr SDL_Window* niku::sdl_window_t::native_handle() const noexcept
{
    return window_;
}

#endif // !VKRNDR_SDL_WINDOW_INCLUDED
