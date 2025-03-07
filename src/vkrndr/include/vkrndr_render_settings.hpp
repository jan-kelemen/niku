#ifndef VKRNDR_RENDER_SETTINGS_INCLUDED
#define VKRNDR_RENDER_SETTINGS_INCLUDED

#include <volk.h>

#include <cstdint>

namespace vkrndr
{
    struct [[nodiscard]] render_settings_t final
    {
        VkFormat preferred_swapchain_format{VK_FORMAT_B8G8R8A8_SRGB};
        VkImageUsageFlags swapchain_flags{VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT};
        VkPresentModeKHR preferred_present_mode{VK_PRESENT_MODE_MAILBOX_KHR};
        uint32_t frames_in_flight{2};

        bool swapchain_maintenance_1_supported{false};
    };
} // namespace vkrndr

#endif
