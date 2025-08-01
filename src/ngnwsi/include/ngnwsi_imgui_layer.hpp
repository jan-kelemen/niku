#ifndef NGNWSI_IMGUI_LAYER_INCLUDED
#define NGNWSI_IMGUI_LAYER_INCLUDED

#include <volk.h>

union SDL_Event;

struct ImGuiContext;

namespace vkrndr
{
    struct image_t;
    struct instance_t;
    struct device_t;
    class execution_port_t;
    class swapchain_t;
} // namespace vkrndr

namespace ngnwsi
{
    class sdl_window_t;
} // namespace ngnwsi

namespace ngnwsi
{
    class [[nodiscard]] imgui_layer_t final
    {
    public:
        imgui_layer_t(sdl_window_t const& window,
            vkrndr::instance_t const& instance,
            vkrndr::device_t& device,
            vkrndr::swapchain_t const& swapchain,
            vkrndr::execution_port_t const& present_queue);

        imgui_layer_t(imgui_layer_t const&) = delete;

        imgui_layer_t(imgui_layer_t&&) noexcept = delete;

    public:
        ~imgui_layer_t();

    public:
        [[nodiscard]] bool handle_event(SDL_Event const& event) const;

        void begin_frame();

        void render(VkCommandBuffer command_buffer,
            vkrndr::image_t const& target_image);

        void end_frame();

        [[nodiscard]] bool enabled() const;

        void set_enabled(bool state);

    public:
        imgui_layer_t& operator=(imgui_layer_t const&) = delete;

        imgui_layer_t& operator=(imgui_layer_t&&) noexcept = delete;

    public:
        vkrndr::device_t* device_;
        VkDescriptorPool descriptor_pool_;

        ImGuiContext* context_;

        bool enabled_{true};
        bool frame_rendered_{false};
    };
} // namespace ngnwsi

inline bool ngnwsi::imgui_layer_t::enabled() const { return enabled_; }

inline void ngnwsi::imgui_layer_t::set_enabled(bool const state)
{
    enabled_ = state;
}

#endif
