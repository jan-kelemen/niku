#ifndef NIKU_IMGUI_LAYER_INCLUDED
#define NIKU_IMGUI_LAYER_INCLUDED

#include <vulkan/vulkan_core.h>

union SDL_Event;

namespace vkrndr
{
    struct image_t;
    struct context_t;
    struct device_t;
    class swap_chain_t;
} // namespace vkrndr

namespace niku
{
    class sdl_window_t;
} // namespace niku

namespace niku
{
    class [[nodiscard]] imgui_layer_t final
    {
    public:
        imgui_layer_t(sdl_window_t const& window,
            vkrndr::context_t const& context,
            vkrndr::device_t& device,
            vkrndr::swap_chain_t const& swap_chain);

        imgui_layer_t(imgui_layer_t const&) = delete;

        imgui_layer_t(imgui_layer_t&&) noexcept = delete;

    public:
        ~imgui_layer_t();

    public:
        [[nodiscard]] bool handle_event(SDL_Event const& event);

        void begin_frame();

        void render(VkCommandBuffer command_buffer,
            vkrndr::image_t const& target_image);

        void end_frame();

        [[nodiscard]] constexpr bool enabled() const;

        constexpr void set_enabled(bool state);

    public:
        imgui_layer_t& operator=(imgui_layer_t const&) = delete;

        imgui_layer_t& operator=(imgui_layer_t&&) noexcept = delete;

    public:
        bool enabled_{true};
        bool frame_rendered_{false};
        vkrndr::device_t* device_;
        VkDescriptorPool descriptor_pool_;
    };
} // namespace niku

constexpr bool niku::imgui_layer_t::enabled() const { return enabled_; }

constexpr void niku::imgui_layer_t::set_enabled(bool const state)
{
    enabled_ = state;
}

#endif
