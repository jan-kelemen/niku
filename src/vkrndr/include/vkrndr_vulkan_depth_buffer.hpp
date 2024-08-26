#ifndef VKRNDR_VULKAN_DEPTH_BUFFER_INCLUDED
#define VKRNDR_VULKAN_DEPTH_BUFFER_INCLUDED

#include <vkrndr_vulkan_image.hpp>

#include <vulkan/vulkan_core.h>

namespace vkrndr
{
    struct vulkan_device;
} // namespace vkrndr

namespace vkrndr
{
    vulkan_image create_depth_buffer(vulkan_device const& device,
        VkExtent2D extent,
        bool with_stencil_component);

    [[nodiscard]] bool has_stencil_component(VkFormat format);
} // namespace vkrndr

#endif
