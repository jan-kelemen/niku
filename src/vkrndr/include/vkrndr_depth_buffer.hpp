#ifndef VKRNDR_DEPTH_BUFFER_INCLUDED
#define VKRNDR_DEPTH_BUFFER_INCLUDED

#include <vkrndr_image.hpp>

#include <vulkan/vulkan_core.h>

namespace vkrndr
{
    struct device_t;
} // namespace vkrndr

namespace vkrndr
{
    image_t create_depth_buffer(device_t const& device,
        VkExtent2D extent,
        bool with_stencil_component);

    [[nodiscard]] bool has_stencil_component(VkFormat format);
} // namespace vkrndr

#endif
