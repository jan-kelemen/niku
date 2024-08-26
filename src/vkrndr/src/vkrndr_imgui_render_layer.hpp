#ifndef VKRNDR_IMGUI_RENDER_LAYER_INCLUDED
#define VKRNDR_IMGUI_RENDER_LAYER_INCLUDED

#include <vulkan/vulkan_core.h>

namespace vkrndr
{
    class vulkan_window;
    struct vulkan_device;
    struct vulkan_context;
    class vulkan_swap_chain;
} // namespace vkrndr

namespace vkrndr
{
    class [[nodiscard]] imgui_render_layer final
    {
    public: // Construction
        imgui_render_layer(vulkan_window* window,
            vulkan_context* context,
            vulkan_device* device,
            vulkan_swap_chain* swap_chain);

        imgui_render_layer(imgui_render_layer const&) = delete;

        imgui_render_layer(imgui_render_layer&&) noexcept = delete;

    public: // Destruction
        ~imgui_render_layer();

    public: // Interface
        void begin_frame();

        void draw(VkCommandBuffer command_buffer,
            VkImage target_image,
            VkImageView target_image_view,
            VkExtent2D extent);

        void end_frame();

    public:
        imgui_render_layer& operator=(imgui_render_layer const&) = delete;

        imgui_render_layer& operator=(imgui_render_layer&&) noexcept = delete;

    private:
        void render();

    private: // Data
        vulkan_window* window_;
        vulkan_device* device_;

        VkDescriptorPool descriptor_pool_;

        bool frame_rendered_{true};
    };
} // namespace vkrndr

#endif
