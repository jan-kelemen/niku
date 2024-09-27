#include <vkrndr_device.hpp>

#include <vkrndr_context.hpp>
#include <vkrndr_execution_port.hpp>
#include <vkrndr_swap_chain.hpp>
#include <vkrndr_utility.hpp>

#include <cppext_pragma_warning.hpp>

#include <spdlog/spdlog.h>

#include <vma_impl.hpp>

#include <volk.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <set>
#include <stdexcept>
#include <string_view>
#include <vector>

// IWYU pragma: no_include <functional>
// IWYU pragma: no_include <initializer_list>
// IWYU pragma: no_include <utility>

namespace
{
    constexpr std::array const device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME};

    DISABLE_WARNING_PUSH
    DISABLE_WARNING_MISSING_FIELD_INITIALIZERS
    constexpr VkPhysicalDeviceFeatures device_features{
        .sampleRateShading = VK_TRUE,
        .wideLines = VK_TRUE,
        .samplerAnisotropy = VK_TRUE};

    constexpr VkPhysicalDeviceVulkan12Features device_12_features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .runtimeDescriptorArray = VK_TRUE};

    constexpr VkPhysicalDeviceVulkan13Features device_13_features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE};
    DISABLE_WARNING_POP

    [[nodiscard]] bool extensions_supported(VkPhysicalDevice device)
    {
        uint32_t count{};
        vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);

        std::vector<VkExtensionProperties> available_extensions{count};
        vkEnumerateDeviceExtensionProperties(device,
            nullptr,
            &count,
            available_extensions.data());

        std::set<std::string_view> required_extensions(
            device_extensions.cbegin(),
            device_extensions.cend());
        for (auto const& extension : available_extensions)
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
            required_extensions.erase(extension.extensionName);
        }

        return required_extensions.empty();
    }

    struct [[nodiscard]] queue_family_t final
    {
        uint32_t index;
        VkQueueFamilyProperties properties;
        bool supports_present;
    };

    [[nodiscard]] std::vector<queue_family_t> get_queue_families(
        VkPhysicalDevice const device,
        VkSurfaceKHR const surface = VK_NULL_HANDLE)
    {
        uint32_t count{};
        vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);

        std::vector<VkQueueFamilyProperties> family_properties{count};
        vkGetPhysicalDeviceQueueFamilyProperties(device,
            &count,
            family_properties.data());

        std::vector<queue_family_t> rv;

        uint32_t index{};
        for (auto const& properties : family_properties)
        {
            rv.emplace_back(index, properties, false);

            if (surface != VK_NULL_HANDLE)
            {
                VkBool32 present_support{VK_FALSE};
                vkrndr::check_result(
                    vkGetPhysicalDeviceSurfaceSupportKHR(device,
                        index,
                        surface,
                        &present_support));

                rv.back().supports_present = present_support == VK_TRUE;
            }

            ++index;
        }

        return rv;
    }

    struct [[nodiscard]] device_families_t final
    {
        queue_family_t present_and_graphics{};
        std::optional<queue_family_t> transfer;
    };

    [[nodiscard]] std::optional<device_families_t>
    is_device_suitable(VkPhysicalDevice device, VkSurfaceKHR surface)
    {
        if (!extensions_supported(device))
        {
            spdlog::info("Device doesn't support required extensions");
            return std::nullopt;
        }

        auto families{get_queue_families(device, surface)};

        device_families_t rv;

        auto const present_and_graphics{std::ranges::find_if(families,
            [](queue_family_t const& f)
            {
                return f.supports_present &&
                    vkrndr::supports_flags(f.properties.queueFlags,
                        VK_QUEUE_GRAPHICS_BIT);
            })};
        if (present_and_graphics != families.cend())
        {
            rv.present_and_graphics = *present_and_graphics;
        }
        else
        {
            spdlog::info("Device doesn't have present queue");
            return std::nullopt;
        }

        auto const transfer{std::ranges::find_if(families,
            [](queue_family_t const& f)
            {
                return vkrndr::supports_flags(f.properties.queueFlags,
                           VK_QUEUE_TRANSFER_BIT) &&
                    !vkrndr::supports_flags(f.properties.queueFlags,
                        VK_QUEUE_GRAPHICS_BIT) &&
                    !vkrndr::supports_flags(f.properties.queueFlags,
                        VK_QUEUE_COMPUTE_BIT);
            })};
        if (transfer != families.cend())
        {
            rv.transfer = *transfer;
        }

        auto const swap_chain{
            vkrndr::query_swap_chain_support(device, surface)};
        bool const swap_chain_adequate = {!swap_chain.surface_formats.empty() &&
            !swap_chain.present_modes.empty()};
        if (!swap_chain_adequate)
        {
            spdlog::info("Device doesn't have adequate swap chain support");
            return std::nullopt;
        }

        VkPhysicalDeviceFeatures supported_features; // NOLINT
        vkGetPhysicalDeviceFeatures(device, &supported_features);
        bool const features_adequate{
            supported_features.samplerAnisotropy == VK_TRUE};
        if (!features_adequate)
        {
            spdlog::info("Device doesn't support required features");
            return std::nullopt;
        }

        return rv;
    }

    [[nodiscard]] VkSampleCountFlagBits max_usable_sample_count(
        VkPhysicalDevice device)
    {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(device, &properties);

        VkSampleCountFlags const counts =
            properties.limits.framebufferColorSampleCounts &
            properties.limits.framebufferDepthSampleCounts;
        for (VkSampleCountFlagBits const count : {VK_SAMPLE_COUNT_64_BIT,
                 VK_SAMPLE_COUNT_32_BIT,
                 VK_SAMPLE_COUNT_16_BIT,
                 VK_SAMPLE_COUNT_8_BIT,
                 VK_SAMPLE_COUNT_4_BIT,
                 VK_SAMPLE_COUNT_2_BIT})
        {
            if ((static_cast<int>(counts) & count) != 0)
            {
                return count;
            }
        }
        return VK_SAMPLE_COUNT_1_BIT;
    }
} // namespace

vkrndr::device_t vkrndr::create_device(context_t const& context)
{
    uint32_t count{};
    vkEnumeratePhysicalDevices(context.instance, &count, nullptr);
    if (count == 0)
    {
        throw std::runtime_error{"failed to find GPUs with Vulkan support!"};
    }

    std::vector<VkPhysicalDevice> devices{count};
    vkEnumeratePhysicalDevices(context.instance, &count, devices.data());

    device_families_t device_families;
    auto const device_it{std::ranges::find_if(devices,
        [&context, &device_families](auto const& device) mutable
        {
            if (auto const families{
                    is_device_suitable(device, context.surface)})
            {
                device_families = *families;
                return true;
            }

            return false;
        })};
    if (device_it == devices.cend())
    {
        throw std::runtime_error{"failed to find a suitable GPU!"};
    }

    device_t rv;
    rv.physical = *device_it;
    rv.max_msaa_samples = max_usable_sample_count(rv.physical);

    auto const priority{1.0f};
    std::vector<queue_family_t> unique_families{
        device_families.present_and_graphics};
    if (device_families.transfer)
    {
        unique_families.push_back(*device_families.transfer);
    }

    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    for (auto const& family : unique_families)
    {
        VkDeviceQueueCreateInfo queue_create_info{};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = family.index;
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &priority;

        queue_create_infos.push_back(queue_create_info);
    }

    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = count_cast(queue_create_infos.size());
    create_info.pQueueCreateInfos = queue_create_infos.data();
    create_info.enabledLayerCount = 0;
    create_info.enabledExtensionCount = count_cast(device_extensions.size());
    create_info.ppEnabledExtensionNames = device_extensions.data();
    create_info.pEnabledFeatures = &device_features;

    VkPhysicalDeviceVulkan13Features next_13_features{device_13_features};
    VkPhysicalDeviceVulkan12Features next_12_features{device_12_features};
    next_12_features.pNext = &next_13_features;
    create_info.pNext = &next_12_features;

    check_result(
        vkCreateDevice(*device_it, &create_info, nullptr, &rv.logical));
    volkLoadDevice(rv.logical);

    VmaVulkanFunctions vma_vulkan_functions{};
    vma_vulkan_functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vma_vulkan_functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocator_info{};
    allocator_info.instance = context.instance;
    allocator_info.physicalDevice = rv.physical;
    allocator_info.device = rv.logical;
    allocator_info.vulkanApiVersion = VK_API_VERSION_1_3;
    allocator_info.pVulkanFunctions = &vma_vulkan_functions;

    check_result(vmaCreateAllocator(&allocator_info, &rv.allocator));

    rv.execution_ports.reserve(unique_families.size());
    for (auto const& family : unique_families)
    {
        auto& port{rv.execution_ports.emplace_back(rv.logical,
            family.properties.queueFlags,
            family.index,
            0)};

        if (port.queue_family() == device_families.present_and_graphics.index)
        {
            rv.present_queue = &port;
        }

        if (device_families.transfer &&
            port.queue_family() == device_families.transfer->index)
        {
            rv.transfer_queue = &port;
        }
    }

    if (!rv.transfer_queue)
    {
        rv.transfer_queue = rv.present_queue;
    }

    return rv;
}

void vkrndr::destroy(device_t* const device)
{
    if (device)
    {
        vmaDestroyAllocator(device->allocator);
        vkDestroyDevice(device->logical, nullptr);
    }
}
