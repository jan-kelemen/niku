#ifndef NGNWSI_RENDER_WINDOW_INCLUDED
#define NGNWSI_RENDER_WINDOW_INCLUDED

#include <ngnwsi_sdl_window.hpp>

#include <vkrndr_image.hpp>
#include <vkrndr_swapchain.hpp>

#include <SDL3/SDL_video.h>

#include <volk.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <span>

namespace vkrndr
{
    struct device_t;
} // namespace vkrndr

namespace ngnwsi
{
    class [[nodiscard]] render_window_t
    {
    public:
        render_window_t(char const* title,
            SDL_WindowFlags window_flags,
            int width,
            int height);

        render_window_t(render_window_t const&) = delete;

        render_window_t(render_window_t&& other) noexcept = delete;

    public:
        virtual ~render_window_t() = default;

    public:
        [[nodiscard]] sdl_window_t& platform_window();

        [[nodiscard]] vkrndr::swapchain_t& swapchain();

        [[nodiscard]] VkSurfaceKHR create_surface(VkInstance instance);

        void destroy_surface(VkInstance instance);

        vkrndr::swapchain_t* create_swapchain(vkrndr::device_t& device,
            vkrndr::swapchain_settings_t const& settings);

        void destroy_swapchain();

        [[nodiscard]] std::optional<vkrndr::image_t> acquire_next_image();

        void present(std::span<VkCommandBuffer const> const& command_buffers);

        void resize(uint32_t width, uint32_t height);

    public:
        render_window_t& operator=(render_window_t const&) = delete;

        render_window_t& operator=(render_window_t&&) noexcept = delete;

    protected:
        sdl_window_t window_;
        vkrndr::device_t* device_{};
        std::unique_ptr<vkrndr::swapchain_t> swapchain_;
        uint32_t current_frame_{};
        uint32_t image_index_{};
        uint32_t frames_in_flight_{};
    };
} // namespace ngnwsi

inline ngnwsi::sdl_window_t& ngnwsi::render_window_t::platform_window()
{
    return window_;
}

inline vkrndr::swapchain_t& ngnwsi::render_window_t::swapchain()
{
    return *swapchain_;
}

#endif
