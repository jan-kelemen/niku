#include <vkrndr_features.hpp>

#include <algorithm>
#include <cstdint>

void vkrndr::link_required_feature_chain(feature_chain_t& chain)
{
    chain.device_10_features.pNext = &chain.device_11_features;
    chain.device_11_features.pNext = &chain.device_12_features;
    chain.device_12_features.pNext = &chain.device_13_features;
    chain.device_13_features.pNext = nullptr;
}

void vkrndr::link_optional_feature_chain(feature_chain_t& chain)
{
    link_required_feature_chain(chain);
    chain.device_13_features.pNext = &chain.swapchain_maintenance_1_features;
}

void vkrndr::add_required_feature_flags(feature_flags_t& flags)
{
    // clang-format off
    flags.device_10_flags.append_range(std::to_array({
        &VkPhysicalDeviceFeatures::independentBlend,
        &VkPhysicalDeviceFeatures::sampleRateShading,
        &VkPhysicalDeviceFeatures::wideLines,
        &VkPhysicalDeviceFeatures::samplerAnisotropy,
        &VkPhysicalDeviceFeatures::tessellationShader,
    }));

    flags.device_12_flags.append_range(std::to_array({
        &VkPhysicalDeviceVulkan12Features::shaderSampledImageArrayNonUniformIndexing,
        &VkPhysicalDeviceVulkan12Features::runtimeDescriptorArray,
    }));

    flags.device_13_flags.append_range(std::to_array({
        &VkPhysicalDeviceVulkan13Features::synchronization2,
        &VkPhysicalDeviceVulkan13Features::dynamicRendering,
    }));
    // clang-format on
}

void vkrndr::set_feature_flags_on_chain(feature_chain_t& chain,
    feature_flags_t const& flags)
{
    auto const set_flags = [](auto&& instance, std::ranges::range auto&& range)
    {
        return std::ranges::for_each(range,
            [&instance](auto const value) { instance.*value = VK_TRUE; });
    };

    set_flags(chain.device_10_features.features, flags.device_10_flags);
    set_flags(chain.device_11_features, flags.device_11_flags);
    set_flags(chain.device_12_features, flags.device_12_flags);
    set_flags(chain.device_13_features, flags.device_13_flags);
}

std::vector<VkLayerProperties> vkrndr::query_instance_layers()
{
    uint32_t count{};
    vkEnumerateInstanceLayerProperties(&count, nullptr);

    std::vector<VkLayerProperties> rv{count};
    vkEnumerateInstanceLayerProperties(&count, rv.data());

    return rv;
}

std::vector<VkExtensionProperties> vkrndr::query_instance_extensions(
    char const* const layer_name)
{
    uint32_t count{};
    vkEnumerateInstanceExtensionProperties(layer_name, &count, nullptr);

    std::vector<VkExtensionProperties> rv{count};
    vkEnumerateInstanceExtensionProperties(layer_name, &count, rv.data());

    return rv;
}

std::vector<VkExtensionProperties> vkrndr::query_device_extensions(
    VkPhysicalDevice device,
    char const* const layer_name)
{
    uint32_t count{};
    vkEnumerateDeviceExtensionProperties(device, layer_name, &count, nullptr);

    std::vector<VkExtensionProperties> rv{count};
    vkEnumerateDeviceExtensionProperties(device, layer_name, &count, rv.data());

    return rv;
}
