#ifndef VKRNDR_FORMATS_INCLUDED
#define VKRNDR_FORMATS_INCLUDED

#include <volk.h>

#include <vector>

namespace vkrndr
{
    struct device_t;
} // namespace vkrndr

namespace vkrndr
{
    struct [[nodiscard]] depth_stencil_format_properties_t final
    {
        VkFormat format;
        VkFormatProperties properties;
        bool depth_component;
        bool stencil_component;
    };

    [[nodiscard]] std::vector<depth_stencil_format_properties_t>
    find_supported_depth_stencil_formats(VkPhysicalDevice device,
        bool needs_depth_component,
        bool needs_stencil_component);
} // namespace vkrndr

#endif
