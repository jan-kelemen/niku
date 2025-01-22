#ifndef VKRNDR_FEATURES_INCLUDED
#define VKRNDR_FEATURES_INCLUDED

#include <volk.h>

#include <vector>

namespace vkrndr
{
    [[nodiscard]] std::vector<VkLayerProperties> query_instance_layers();

    [[nodiscard]] std::vector<VkExtensionProperties> query_instance_extensions(
        char const* layer_name = nullptr);

    [[nodiscard]] std::vector<VkExtensionProperties> query_device_extensions(
        VkPhysicalDevice device,
        char const* layer_name = nullptr);
} // namespace vkrndr

#endif
