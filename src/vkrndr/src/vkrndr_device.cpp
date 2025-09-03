#include <spdlog/spdlog.h>
#include <vkrndr_device.hpp>

#include <vkrndr_error_code.hpp>
#include <vkrndr_execution_port.hpp>
#include <vkrndr_features.hpp>
#include <vkrndr_instance.hpp>
#include <vkrndr_utility.hpp>

#include <vma_impl.hpp>

#include <volk.h>

#include <algorithm>
#include <iterator>
#include <ranges>
#include <string_view>
#include <vector>

// IWYU pragma: no_include <fmt/base.h>
// IWYU pragma: no_include <fmt/format.h>
// IWYU pragma: no_include <functional>
// IWYU pragma: no_include <initializer_list>
// IWYU pragma: no_include <utility>

namespace
{
    [[nodiscard]] VkSampleCountFlagBits max_usable_sample_count(
        VkPhysicalDevice device)
    {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(device, &properties);
        VkSampleCountFlags const counts =
            properties.limits.framebufferColorSampleCounts &
            properties.limits.framebufferDepthSampleCounts;

        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
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
        }
        else
        {
            for (VkSampleCountFlagBits const count :
                {VK_SAMPLE_COUNT_4_BIT, VK_SAMPLE_COUNT_2_BIT})
            {
                if ((static_cast<int>(counts) & count) != 0)
                {
                    return count;
                }
            }
        }

        return VK_SAMPLE_COUNT_1_BIT;
    }

    struct [[nodiscard]] device_create_info_t final
    {
        void const* chain{};

        VkPhysicalDevice device;
        std::span<char const* const> extensions;
        std::span<vkrndr::queue_family_t const> queues;
    };

    [[nodiscard]] std::expected<vkrndr::device_ptr_t, std::error_code>
    create_device_impl(vkrndr::instance_t const& instance,
        device_create_info_t const& create_info)
    {
        vkrndr::device_ptr_t rv{new vkrndr::device_t};

        rv->physical_device = create_info.device;
        rv->max_msaa_samples = max_usable_sample_count(*rv);

        float const priority{1.0f};
        std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
        queue_create_infos.reserve(create_info.queues.size());
        for (auto const& family : create_info.queues)
        {
            VkDeviceQueueCreateInfo const queue_create_info{
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = family.index,
                .queueCount = 1,
                .pQueuePriorities = &priority};

            queue_create_infos.push_back(queue_create_info);
        }

        VkDeviceCreateInfo const ci{
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = create_info.chain,
            .queueCreateInfoCount =
                vkrndr::count_cast(queue_create_infos.size()),
            .pQueueCreateInfos = queue_create_infos.data(),
            .enabledExtensionCount =
                vkrndr::count_cast(create_info.extensions.size()),
            .ppEnabledExtensionNames = create_info.extensions.data()};

        if (VkResult const result{vkCreateDevice(create_info.device,
                &ci,
                nullptr,
                &rv->logical_device)};
            result != VK_SUCCESS)
        {
            spdlog::error("{}", vkrndr::make_error_code(result).message());
            return std::unexpected{vkrndr::make_error_code(result)};
        }

        std::ranges::copy(create_info.extensions,
            std::inserter(rv->extensions, rv->extensions.begin()));

        volkLoadDevice(*rv);

        VmaVulkanFunctions const vma_vulkan_functions{
            .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
            .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
        };

        VmaAllocatorCreateInfo allocator_info{
            .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
            .physicalDevice = *rv,
            .device = *rv,
            .pVulkanFunctions = &vma_vulkan_functions,
            .instance = instance.handle,
            .vulkanApiVersion = VK_API_VERSION_1_3,
        };

        if (is_device_extension_enabled(VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME,
                *rv))
        {
            allocator_info.flags |=
                VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT;
        }

        if (is_device_extension_enabled(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
                *rv))
        {
            allocator_info.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
        }

        if (VkResult const result{
                vmaCreateAllocator(&allocator_info, &rv->allocator)};
            result != VK_SUCCESS)
        {
            return std::unexpected{vkrndr::make_error_code(result)};
        }

        rv->execution_ports.reserve(queue_create_infos.size());
        for (auto const& family : create_info.queues)
        {
            rv->execution_ports.emplace_back(*rv,
                family.properties.queueFlags,
                family.index,
                0,
                family.supports_present);
        }

        return rv;
    }

    [[nodiscard]] std::vector<vkrndr::physical_device_features_t>
    filter_physical_devices(vkrndr::instance_t const& instance,
        std::span<char const* const> const& required_extensions,
        vkrndr::feature_flags_t const& required_feature_flags,
        VkSurfaceKHR const surface)
    {
        std::vector<vkrndr::physical_device_features_t> rv;

        for (vkrndr::physical_device_features_t& device :
            vkrndr::query_available_physical_devices(instance, surface))
        {
            bool const device_extensions_supported{
                std::ranges::all_of(required_extensions,
                    [de = device.extensions](std::string_view name)
                    {
                        return std::ranges::contains(de,
                            name,
                            &VkExtensionProperties::extensionName);
                    })};
            if (!device_extensions_supported)
            {
                continue;
            }

            if (!vkrndr::check_feature_flags(device.features,
                    required_feature_flags))
            {
                continue;
            }

            if (surface != VK_NULL_HANDLE)
            {
                bool const has_present_queue{std::ranges::any_of(
                    device.queue_families,
                    [](vkrndr::queue_family_t const& family)
                    {
                        return family.supports_present &&
                            vkrndr::supports_flags(family.properties.queueFlags,
                                VK_QUEUE_GRAPHICS_BIT);
                    })};
                if (!has_present_queue)
                {
                    continue;
                }

                if (!device.swapchain_support ||
                    device.swapchain_support->surface_formats.empty() ||
                    device.swapchain_support->present_modes.empty())
                {
                    continue;
                }
            }

            rv.emplace_back(std::move(device));
        }

        return rv;
    }

    [[nodiscard]] auto pick_device_by_type(
        std::ranges::forward_range auto&& devices)
    {
        for (auto it{std::begin(devices)}; it != std::end(devices); ++it)
        {
            if (it->properties.deviceType ==
                VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            {
                return it;
            }
        }

        for (auto it{std::begin(devices)}; it != std::end(devices); ++it)
        {
            if (it->properties.deviceType ==
                VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
            {
                return it;
            }
        }

        for (auto it{std::begin(devices)}; it != std::end(devices); ++it)
        {
            if (it->properties.deviceType ==
                VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU)
            {
                return it;
            }
        }

        return std::begin(devices);
    }
} // namespace

vkrndr::device_t::~device_t()
{
    vmaDestroyAllocator(allocator);

    vkDestroyDevice(logical_device, nullptr);
}

bool vkrndr::is_device_extension_enabled(char const* const extension_name,
    device_t const& device)
{
    return device.extensions.contains(extension_name);
}

std::expected<vkrndr::device_ptr_t, std::error_code> vkrndr::create_device(
    instance_t const& instance,
    std::span<char const* const> const& extensions,
    physical_device_features_t const& features,
    std::span<queue_family_t const> const& queue_families)
{
    feature_flags_t required_flags;
    add_required_feature_flags(required_flags);

    feature_chain_t effective_features;
    set_feature_flags_on_chain(effective_features, required_flags);
    link_required_feature_chain(effective_features);

    return create_device(instance,
        extensions,
        features,
        effective_features,
        queue_families);
}

std::expected<vkrndr::device_ptr_t, std::error_code> vkrndr::create_device(
    instance_t const& instance,
    std::span<char const* const> const& extensions,
    physical_device_features_t const& features,
    feature_chain_t& feature_chain,
    std::span<queue_family_t const> const& queue_families)
{
    std::vector<char const*> effective_extensions{std::cbegin(extensions),
        std::cend(extensions)};

    if (enable_extension_for_device(
            VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME,
            features,
            feature_chain))
    {
        effective_extensions.push_back(
            VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME);
    }

    if (enable_extension_for_device(VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME,
            features,
            feature_chain))
    {
        effective_extensions.push_back(VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME);
    }

    if (enable_extension_for_device(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
            features,
            feature_chain))
    {
        effective_extensions.push_back(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
    }

    device_create_info_t const device_create_info{
        .chain = &feature_chain.device_10_features,
        .device = features.device,
        .extensions = effective_extensions,
        .queues = queue_families};
    return create_device_impl(instance, device_create_info);
}

std::optional<vkrndr::physical_device_features_t>
vkrndr::pick_best_physical_device(instance_t const& instance,
    std::span<char const* const> const& extensions,
    VkSurfaceKHR surface)
{
    feature_flags_t required_flags;
    add_required_feature_flags(required_flags);

    return pick_best_physical_device(instance,
        extensions,
        required_flags,
        surface);
}

std::optional<vkrndr::physical_device_features_t>
vkrndr::pick_best_physical_device(instance_t const& instance,
    std::span<char const* const> const& extensions,
    feature_flags_t const& required_flags,
    VkSurfaceKHR surface)
{
    auto physical_devices{
        filter_physical_devices(instance, extensions, required_flags, surface)};
    auto physical_device_it{pick_device_by_type(physical_devices)};
    if (physical_device_it == std::end(physical_devices))
    {
        return std::nullopt;
    }

    return std::make_optional(std::move(*physical_device_it));
}
