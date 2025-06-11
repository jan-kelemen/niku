#ifndef VKRNDR_FEATURES_INCLUDED
#define VKRNDR_FEATURES_INCLUDED

#include <volk.h>

#include <vulkan/utility/vk_struct_helper.hpp>

#include <array>
#include <optional>
#include <vector>

namespace vkrndr
{
    struct [[nodiscard]] feature_chain_t final
    {
        VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT
            swapchain_maintenance_1_features{
                .sType = vku::GetSType<
                    VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT>()};

        VkPhysicalDeviceVulkan13Features device_13_features{
            .sType = vku::GetSType<VkPhysicalDeviceVulkan13Features>()};

        VkPhysicalDeviceVulkan12Features device_12_features{
            .sType = vku::GetSType<VkPhysicalDeviceVulkan12Features>()};

        VkPhysicalDeviceVulkan11Features device_11_features{
            .sType = vku::GetSType<VkPhysicalDeviceVulkan11Features>()};

        VkPhysicalDeviceFeatures2 device_10_features{
            .sType = vku::GetSType<VkPhysicalDeviceFeatures2>()};
    };

    void link_required_feature_chain(feature_chain_t& chain);

    void link_optional_feature_chain(feature_chain_t& chain);

    struct [[nodiscard]] feature_flags_t final
    {
        std::vector<VkBool32 VkPhysicalDeviceFeatures::*> device_10_flags;

        std::vector<VkBool32 VkPhysicalDeviceVulkan11Features::*>
            device_11_flags;

        std::vector<VkBool32 VkPhysicalDeviceVulkan12Features::*>
            device_12_flags;

        std::vector<VkBool32 VkPhysicalDeviceVulkan13Features::*>
            device_13_flags;

        std::optional<
            VkBool32 VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT::*>
            swapchain_maintenance_1_flags;
    };

    void add_required_feature_flags(feature_flags_t& flags);

    void set_feature_flags_on_chain(feature_chain_t& chain,
        feature_flags_t const& flags);

    [[nodiscard]] std::vector<VkLayerProperties> query_instance_layers();

    [[nodiscard]] std::vector<VkExtensionProperties> query_instance_extensions(
        char const* layer_name = nullptr);

    [[nodiscard]] std::vector<VkExtensionProperties> query_device_extensions(
        VkPhysicalDevice device,
        char const* layer_name = nullptr);
} // namespace vkrndr

#endif
