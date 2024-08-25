#include <vulkan_device.hpp>

#include <vulkan_context.hpp>
#include <vulkan_queue.hpp>
#include <vulkan_swap_chain.hpp>
#include <vulkan_utility.hpp>

#include <cppext_pragma_warning.hpp>

#include <vma_impl.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <ranges>
#include <set>
#include <stdexcept>
#include <string_view>
#include <vector>

// IWYU pragma: no_include <functional>
// IWYU pragma: no_include <initializer_list>

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

    [[nodiscard]] bool is_device_suitable(VkPhysicalDevice device,
        VkSurfaceKHR surface,
        vkrndr::queue_families& indices)
    {
        if (!extensions_supported(device))
        {
            return false;
        }

        auto families{vkrndr::find_queue_families(device, surface)};
        if (!families.present_family.has_value())
        {
            return false;
        }

        auto const swap_chain{
            vkrndr::query_swap_chain_support(device, surface)};
        bool const swap_chain_adequate = {!swap_chain.surface_formats.empty() &&
            !swap_chain.present_modes.empty()};
        if (!swap_chain_adequate)
        {
            return false;
        }

        VkPhysicalDeviceFeatures supported_features; // NOLINT
        vkGetPhysicalDeviceFeatures(device, &supported_features);
        bool const features_adequate{
            supported_features.samplerAnisotropy == VK_TRUE};
        if (!features_adequate)
        {
            return false;
        }

        indices = families;

        return true;
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

vkrndr::vulkan_device vkrndr::create_device(vulkan_context const& context)
{
    uint32_t count{};
    vkEnumeratePhysicalDevices(context.instance, &count, nullptr);
    if (count == 0)
    {
        throw std::runtime_error{"failed to find GPUs with Vulkan support!"};
    }

    std::vector<VkPhysicalDevice> devices{count};
    vkEnumeratePhysicalDevices(context.instance, &count, devices.data());

    queue_families device_indices;
    auto const device_it{std::ranges::find_if(devices,
        [&context, &device_indices](auto const& device) mutable {
            return is_device_suitable(device, context.surface, device_indices);
        })};
    if (device_it == devices.cend())
    {
        throw std::runtime_error{"failed to find a suitable GPU!"};
    }

    vulkan_device rv;
    rv.physical = *device_it;
    rv.max_msaa_samples = max_usable_sample_count(rv.physical);

    auto const present_family{device_indices.present_family.value_or(0)};
    auto const transfer_family{
        device_indices.dedicated_transfer_family.value_or(present_family)};

    auto const priority{1.0f};
    std::set<uint32_t> const unique_families{present_family, transfer_family};
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    for (uint32_t const family : unique_families)
    {
        VkDeviceQueueCreateInfo queue_create_info{};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = family;
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
    create_info.pNext = &device_13_features;

    check_result(
        vkCreateDevice(*device_it, &create_info, nullptr, &rv.logical));

    VmaAllocatorCreateInfo allocator_info{};
    allocator_info.instance = context.instance;
    allocator_info.physicalDevice = rv.physical;
    allocator_info.device = rv.logical;
    allocator_info.vulkanApiVersion = VK_API_VERSION_1_3;

    check_result(vmaCreateAllocator(&allocator_info, &rv.allocator));

    rv.queues.reserve(unique_families.size());
    for (uint32_t const family : unique_families)
    {
        auto& queue{rv.queues.emplace_back(create_queue(&rv, family, 0))};

        if (queue.family == present_family)
        {
            rv.present_queue = &queue;
        }

        if (queue.family == transfer_family)
        {
            rv.transfer_queue = &queue;
        }
    }

    return rv;
}

void vkrndr::destroy(vulkan_device* const device)
{
    if (device)
    {
        vmaDestroyAllocator(device->allocator);
        vkDestroyDevice(device->logical, nullptr);
    }
}
