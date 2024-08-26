#ifndef VKRNDR_VULKAN_CONTEXT_INCLUDED
#define VKRNDR_VULKAN_CONTEXT_INCLUDED

#include <vulkan/vulkan_core.h>

namespace vkrndr
{
    class vulkan_window;
} // namespace vkrndr

namespace vkrndr
{
    struct [[nodiscard]] vulkan_context final
    {
        VkInstance instance{VK_NULL_HANDLE};
        VkSurfaceKHR surface{VK_NULL_HANDLE};
        VkDebugUtilsMessengerEXT debug_messenger{VK_NULL_HANDLE};
    };

    vulkan_context create_context(vulkan_window const* window,
        bool setup_validation_layers);

    void destroy(vulkan_context* context);
} // namespace vkrndr

#endif // !VKRNDR_VULKAN_CONTEXT_INCLUDED
