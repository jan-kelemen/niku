#ifndef NIKU_SDL_WINDOW_INCLUDED
#define NIKU_SDL_WINDOW_INCLUDED

#include <vulkan_window.hpp>

#include <SDL2/SDL_video.h>

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <string_view>
#include <vector>

namespace niku
{
    class [[nodiscard]] sdl_guard final
    {
    public: // Construction
        explicit sdl_guard(uint32_t flags);

        sdl_guard(sdl_guard const&) = delete;

        sdl_guard(sdl_guard&&) noexcept = delete;

    public: // Destruction
        ~sdl_guard();

    public: // Operators
        sdl_guard& operator=(sdl_guard const&) = delete;

        sdl_guard& operator=(sdl_guard&&) = delete;
    };

    class [[nodiscard]] sdl_window final : public vkrndr::vulkan_window
    {
    public: // Construction
        sdl_window(std::string_view title,
            SDL_WindowFlags window_flags,
            bool centered,
            int width,
            int height);

        sdl_window(sdl_window const&) = delete;

        sdl_window(sdl_window&&) noexcept = delete;

    public: // Destruction
        ~sdl_window() override;

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

        void init_imgui() override;

        void new_imgui_frame() override;

        void shutdown_imgui() override;

    public: // Operators
        sdl_window& operator=(sdl_window const&) = delete;

        sdl_window& operator=(sdl_window&&) noexcept = delete;

    private: // Data
        SDL_Window* window_;
    };
} // namespace niku

[[nodiscard]]
constexpr SDL_Window* niku::sdl_window::native_handle() const noexcept
{
    return window_;
}

#endif // !VKRNDR_SDL_WINDOW_INCLUDED
