#ifndef VKRNDR_FEATURES_INCLUDED
#define VKRNDR_FEATURES_INCLUDED

#include <volk.h>

#include <vulkan/utility/vk_struct_helper.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace vkrndr
{
    [[nodiscard]] std::vector<VkLayerProperties> query_instance_layers();

    [[nodiscard]] std::vector<VkExtensionProperties> query_instance_extensions(
        char const* layer_name = nullptr);

    struct [[nodiscard]] feature_chain_t final
    {
        VkPhysicalDeviceFeatures2 device_10_features{
            .sType = vku::GetSType<VkPhysicalDeviceFeatures2>()};

        VkPhysicalDeviceVulkan11Features device_11_features{
            .sType = vku::GetSType<VkPhysicalDeviceVulkan11Features>()};

        VkPhysicalDeviceVulkan12Features device_12_features{
            .sType = vku::GetSType<VkPhysicalDeviceVulkan12Features>()};

        VkPhysicalDeviceVulkan13Features device_13_features{
            .sType = vku::GetSType<VkPhysicalDeviceVulkan13Features>()};

        VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT
            swapchain_maintenance_1_features{
                .sType = vku::GetSType<
                    VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT>()};

        VkPhysicalDeviceMemoryPriorityFeaturesEXT memory_priority_features{
            .sType =
                vku::GetSType<VkPhysicalDeviceMemoryPriorityFeaturesEXT>()};

        VkPhysicalDeviceAccelerationStructureFeaturesKHR
            acceleration_structure_features{
                .sType = vku::GetSType<
                    VkPhysicalDeviceAccelerationStructureFeaturesKHR>()};

        VkPhysicalDeviceRayTracingPipelineFeaturesKHR
            ray_tracing_pipeline_features{
                .sType = vku::GetSType<
                    VkPhysicalDeviceRayTracingPipelineFeaturesKHR>()};

        VkPhysicalDeviceShaderRelaxedExtendedInstructionFeaturesKHR
            relaxed_extended_instruction_features{
                .sType = vku::GetSType<
                    VkPhysicalDeviceShaderRelaxedExtendedInstructionFeaturesKHR>()};
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

        std::optional<VkBool32 VkPhysicalDeviceMemoryPriorityFeaturesEXT::*>
            memory_priority_flags;

        std::vector<VkBool32 VkPhysicalDeviceMemoryBudgetPropertiesEXT::*>
            memory_budget_flags;

        std::vector<
            VkBool32 VkPhysicalDeviceAccelerationStructureFeaturesKHR::*>
            acceleration_structure_flags;

        std::vector<VkBool32 VkPhysicalDeviceRayTracingPipelineFeaturesKHR::*>
            ray_tracing_pipeline_flags;

        std::optional<VkBool32
                VkPhysicalDeviceShaderRelaxedExtendedInstructionFeaturesKHR ::*>
            relaxed_extended_instruction_flags;
    };

    void add_required_feature_flags(feature_flags_t& flags);

    void set_feature_flags_on_chain(feature_chain_t& chain,
        feature_flags_t const& flags);

    [[nodiscard]] bool check_feature_flags(feature_chain_t const& chain,
        feature_flags_t const& flags);

    struct [[nodiscard]] queue_family_t final
    {
        uint32_t index;
        VkQueueFamilyProperties properties;
        bool supports_present;
    };

    struct [[nodiscard]] swapchain_support_t final
    {
        VkSurfaceCapabilitiesKHR capabilities{};
        std::vector<VkSurfaceFormatKHR> surface_formats;
        std::vector<VkPresentModeKHR> present_modes;
    };

    swapchain_support_t query_swapchain_support(VkPhysicalDevice device,
        VkSurfaceKHR surface);

    struct [[nodiscard]] physical_device_features_t final
    {
        VkPhysicalDevice device;
        VkPhysicalDeviceProperties properties;
        std::vector<VkExtensionProperties> extensions;
        std::vector<vkrndr::queue_family_t> queue_families;
        std::optional<vkrndr::swapchain_support_t> swapchain_support;
        vkrndr::feature_chain_t features;
    };

    [[nodiscard]] std::vector<physical_device_features_t>
    query_available_physical_devices(VkInstance instance,
        VkSurfaceKHR surface = VK_NULL_HANDLE);

    [[nodiscard]] bool enable_extension_for_device(char const* extension_name,
        physical_device_features_t const& device,
        vkrndr::feature_chain_t& chain);

    template<typename T>
    [[nodiscard]] T get_device_properties(VkPhysicalDevice device);
} // namespace vkrndr

template<typename T>
T vkrndr::get_device_properties(VkPhysicalDevice const device)
{
    T rv{.sType = vku::GetSType<T>()};

    VkPhysicalDeviceProperties2 props{
        .sType = vku::GetSType<VkPhysicalDeviceProperties2>(),
        .pNext = &rv};

    vkGetPhysicalDeviceProperties2(device, &props);

    return rv;
}

#endif
