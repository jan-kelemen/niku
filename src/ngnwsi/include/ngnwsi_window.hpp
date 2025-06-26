#ifndef NGNWSI_WINDOW_INCLUDED
#define NGNWSI_WINDOW_INCLUDED

#include <ngnwsi_sdl_window.hpp>

#include <vkrndr_image.hpp>

#include <SDL3/SDL_video.h>

#include <volk.h>

#include <cstdint>
#include <memory>

union SDL_Event;

namespace vkrndr
{
    struct device_t;
    struct render_settings_t;
    class swap_chain_t;
} // namespace vkrndr

namespace ngnwsi
{
    class [[nodiscard]] window_t
    {
    public:
        window_t(char const* title,
            SDL_WindowFlags window_flags,
            int width,
            int height);

        window_t(window_t const&) = delete;

        window_t(window_t&&) noexcept = default;

    public:
        virtual ~window_t();

    public:
        [[nodiscard]] uint32_t id() const;

        [[nodiscard]] VkFormat image_format() const;

        [[nodiscard]] uint32_t image_count() const;

        [[nodiscard]] uint32_t frames_in_flight() const;

        [[nodiscard]] VkExtent2D extent() const;

        [[nodiscard]] vkrndr::image_t swapchain_image() const;

        [[nodiscard]] sdl_window_t const& native_window() const;

        VkSurfaceKHR create_surface(VkInstance instance);

        vkrndr::swap_chain_t* create_swapchain(vkrndr::device_t& device,
            vkrndr::render_settings_t const&);

    public:
        virtual void handle_event(SDL_Event const& event) { }

        [[nodiscard]] bool acquire_image();

        void present(std::span<VkCommandBuffer const> const& command_buffers);

    public:
        window_t& operator=(window_t const&) = delete;

        window_t& operator=(window_t&&) noexcept = default;

    protected:
        sdl_window_t window_;
        VkInstance instance_{VK_NULL_HANDLE};

        vkrndr::device_t* device_{nullptr};

        uint32_t frames_in_flight_{};
        uint32_t frame_index_{};
        uint32_t image_index_{};
        bool needs_refresh_{};
        std::unique_ptr<vkrndr::swap_chain_t> swapchain_;
    };
} // namespace ngnwsi

#endif
