#ifndef NGNWSI_SDL_WINDOW_INCLUDED
#define NGNWSI_SDL_WINDOW_INCLUDED

#include <vkrndr_window.hpp>

#include <SDL3/SDL_video.h>

#include <volk.h>

#include <vector>

namespace ngnwsi
{
    class [[nodiscard]] sdl_window_t final : public vkrndr::window_t
    {
    public:
        [[nodiscard]] static std::vector<char const*> required_extensions();

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

        [[nodiscard]] SDL_WindowID window_id() const noexcept;

        [[nodiscard]] bool is_minimized() const noexcept;

        [[nodiscard]] bool is_focused() const noexcept;

    public: // window_t implementation
        [[nodiscard]] VkSurfaceKHR create_surface(VkInstance instance) override;

        void destroy_surface(VkInstance instance) override;

        [[nodiscard]] VkSurfaceKHR surface() const override;

        [[nodiscard]] VkExtent2D swap_extent() const override;

    public: // Operators
        sdl_window_t& operator=(sdl_window_t const&) = delete;

        sdl_window_t& operator=(sdl_window_t&&) noexcept = delete;

    private: // Data
        SDL_Window* window_;
        VkSurfaceKHR surface_{VK_NULL_HANDLE};
    };
} // namespace ngnwsi

[[nodiscard]]
constexpr SDL_Window* ngnwsi::sdl_window_t::native_handle() const noexcept
{
    return window_;
}

#endif // !VKRNDR_SDL_WINDOW_INCLUDED
