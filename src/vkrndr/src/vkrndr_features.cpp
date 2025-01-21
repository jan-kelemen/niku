#include <vkrndr_features.hpp>

#include <cstdint>

std::vector<VkLayerProperties> vkrndr::query_instance_layers()
{
    uint32_t count{};
    vkEnumerateInstanceLayerProperties(&count, nullptr);

    std::vector<VkLayerProperties> rv{count};
    vkEnumerateInstanceLayerProperties(&count, rv.data());

    return rv;
}

std::vector<VkExtensionProperties> vkrndr::query_instance_extensions(
    char const* layer_name)
{
    uint32_t count{};
    vkEnumerateInstanceExtensionProperties(layer_name, &count, nullptr);

    std::vector<VkExtensionProperties> rv{count};
    vkEnumerateInstanceExtensionProperties(layer_name, &count, rv.data());

    return rv;
}
