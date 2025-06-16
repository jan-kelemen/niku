#include <vkrndr_features.hpp>

#include <vkrndr_utility.hpp>

#include <algorithm>
#include <cstdint>
#include <string_view>

namespace
{
    [[nodiscard]] std::vector<VkExtensionProperties> query_device_extensions(
        VkPhysicalDevice device,
        char const* layer_name = nullptr)
    {
        uint32_t count{};
        vkEnumerateDeviceExtensionProperties(device,
            layer_name,
            &count,
            nullptr);

        std::vector<VkExtensionProperties> rv{count};
        vkEnumerateDeviceExtensionProperties(device,
            layer_name,
            &count,
            rv.data());

        return rv;
    }

    [[nodiscard]] std::vector<vkrndr::queue_family_t> query_queue_families(
        VkPhysicalDevice device,
        VkSurfaceKHR surface = VK_NULL_HANDLE)
    {
        uint32_t count{};
        vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);

        std::vector<VkQueueFamilyProperties> family_properties{count};
        vkGetPhysicalDeviceQueueFamilyProperties(device,
            &count,
            family_properties.data());

        std::vector<vkrndr::queue_family_t> rv;
        rv.reserve(family_properties.size());

        uint32_t index{};
        for (auto const& properties : family_properties)
        {
            if (surface != VK_NULL_HANDLE)
            {
                VkBool32 present_support{VK_FALSE};
                vkGetPhysicalDeviceSurfaceSupportKHR(device,
                    index,
                    surface,
                    &present_support);

                rv.emplace_back(index, properties, present_support == VK_TRUE);
            }
            else
            {
                rv.emplace_back(index, properties, false);
            }

            ++index;
        }

        return rv;
    }

    [[nodiscard]] std::vector<VkPhysicalDevice> query_physical_devices(
        VkInstance instance)
    {
        uint32_t count{};
        vkEnumeratePhysicalDevices(instance, &count, nullptr);

        std::vector<VkPhysicalDevice> rv{count};
        vkEnumeratePhysicalDevices(instance, &count, rv.data());

        return rv;
    }
} // namespace

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
    flags.device_10_flags.insert(flags.device_10_flags.end(),
        {
            &VkPhysicalDeviceFeatures::independentBlend,
            &VkPhysicalDeviceFeatures::sampleRateShading,
            &VkPhysicalDeviceFeatures::wideLines,
            &VkPhysicalDeviceFeatures::samplerAnisotropy,
            &VkPhysicalDeviceFeatures::tessellationShader,
        });

    flags.device_12_flags.insert(flags.device_12_flags.end(),
        {
            // clang-format off
            &VkPhysicalDeviceVulkan12Features::shaderSampledImageArrayNonUniformIndexing,
            &VkPhysicalDeviceVulkan12Features::runtimeDescriptorArray,
            // clang-format on
        });

    flags.device_13_flags.insert(flags.device_13_flags.end(),
        {
            &VkPhysicalDeviceVulkan13Features::synchronization2,
            &VkPhysicalDeviceVulkan13Features::dynamicRendering,
        });
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

bool vkrndr::check_feature_flags(feature_chain_t const& chain,
    feature_flags_t const& flags)
{
    auto const all_true = [](auto&& instance, std::ranges::range auto&& range)
    {
        return std::all_of(std::begin(range),
            std::end(range),
            [&instance](auto const value)
            { return instance.*value == VK_TRUE; });
    };

    if (!all_true(chain.device_10_features.features, flags.device_10_flags))
    {
        return false;
    }

    if (!all_true(chain.device_11_features, flags.device_11_flags))
    {
        return false;
    }

    if (!all_true(chain.device_12_features, flags.device_12_flags))
    {
        return false;
    }

    if (!all_true(chain.device_13_features, flags.device_13_flags))
    {
        return false;
    }

    return true;
}

vkrndr::swap_chain_support_t
vkrndr::query_swap_chain_support(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    swap_chain_support_t rv;

    check_result(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device,
        surface,
        &rv.capabilities));

    uint32_t format_count{};
    check_result(vkGetPhysicalDeviceSurfaceFormatsKHR(device,
        surface,
        &format_count,
        nullptr));
    if (format_count != 0)
    {
        rv.surface_formats.resize(format_count);
        check_result(vkGetPhysicalDeviceSurfaceFormatsKHR(device,
            surface,
            &format_count,
            rv.surface_formats.data()));
    }

    uint32_t present_count{};
    check_result(vkGetPhysicalDeviceSurfacePresentModesKHR(device,
        surface,
        &present_count,
        nullptr));
    if (present_count != 0)
    {
        rv.present_modes.resize(present_count);
        check_result(vkGetPhysicalDeviceSurfacePresentModesKHR(device,
            surface,
            &present_count,
            rv.present_modes.data()));
    }

    return rv;
}

std::vector<vkrndr::physical_device_features_t>
vkrndr::query_available_physical_devices(VkInstance instance,
    VkSurfaceKHR surface)
{
    std::vector<physical_device_features_t> rv;
    for (VkPhysicalDevice device : query_physical_devices(instance))
    {
        physical_device_features_t current{.device = device,
            .extensions = query_device_extensions(device),
            .queue_families = query_queue_families(device, surface)};

        link_optional_feature_chain(current.features);
        vkGetPhysicalDeviceFeatures2(device,
            &current.features.device_10_features);

        vkGetPhysicalDeviceProperties(device, &current.properties);

        if (surface != VK_NULL_HANDLE)
        {
            current.swap_chain_support =
                query_swap_chain_support(device, surface);
        }

        rv.emplace_back(std::move(current));
    }

    return rv;
}

bool vkrndr::enable_extension_for_device(char const* const extension_name,
    physical_device_features_t const& device,
    vkrndr::feature_chain_t& chain)
{
    static constexpr std::string_view swapchain_maintenance_1{
        VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME};

    if (extension_name == swapchain_maintenance_1)
    {
        if (device.features.swapchain_maintenance_1_features
                .swapchainMaintenance1 != VK_TRUE)
        {
            return false;
        }

        if (!std::ranges::contains(device.extensions,
                swapchain_maintenance_1,
                &VkExtensionProperties::extensionName))
        {
            return false;
        }

        chain.swapchain_maintenance_1_features.swapchainMaintenance1 = VK_TRUE;
        chain.swapchain_maintenance_1_features.pNext =
            std::exchange(chain.device_13_features.pNext,
                &chain.swapchain_maintenance_1_features);

        return true;
    }

    return false;
}
