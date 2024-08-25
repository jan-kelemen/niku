#ifndef VKRNDR_RENDER_SETTINGS_INCLUDED
#define VKRNDR_RENDER_SETTINGS_INCLUDED

#include <vulkan/vulkan_core.h>

namespace vkrndr
{
    struct [[nodiscard]] render_settings final
    {
        VkFormat preferred_swapchain_format{VK_FORMAT_B8G8R8A8_SRGB};
        VkPresentModeKHR preferred_present_mode{VK_PRESENT_MODE_MAILBOX_KHR};
    };
} // namespace vkrndr

#endif
