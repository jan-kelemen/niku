#ifndef VKRNDR_IMGUI_RENDER_LAYER_INCLUDED
#define VKRNDR_IMGUI_RENDER_LAYER_INCLUDED

#include <vulkan/vulkan_core.h>

namespace vkrndr
{
    class window_t;
    struct device_t;
    struct context_t;
    class swap_chain_t;
} // namespace vkrndr

namespace vkrndr
{
    class [[nodiscard]] imgui_render_layer_t final
    {
    public: // Construction
        imgui_render_layer_t(window_t* window,
            context_t* context,
            device_t* device,
            swap_chain_t* swap_chain);

        imgui_render_layer_t(imgui_render_layer_t const&) = delete;

        imgui_render_layer_t(imgui_render_layer_t&&) noexcept = delete;

    public: // Destruction
        ~imgui_render_layer_t();

    public: // Interface
        void begin_frame();

        void draw(VkCommandBuffer command_buffer,
            VkImage target_image,
            VkImageView target_image_view,
            VkExtent2D extent);

        void end_frame();

    public:
        imgui_render_layer_t& operator=(imgui_render_layer_t const&) = delete;

        imgui_render_layer_t& operator=(
            imgui_render_layer_t&&) noexcept = delete;

    private:
        void render();

    private: // Data
        window_t* window_;
        device_t* device_;

        VkDescriptorPool descriptor_pool_;

        bool frame_rendered_{true};
    };
} // namespace vkrndr

#endif
