#include <vkrndr_device.hpp>

#include <vkrndr_context.hpp>
#include <vkrndr_execution_port.hpp>
#include <vkrndr_features.hpp>
#include <vkrndr_swap_chain.hpp>
#include <vkrndr_utility.hpp>

#include <cppext_pragma_warning.hpp>

#include <boost/scope/scope_fail.hpp>

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

// IWYU pragma: no_include <boost/scope/exception_checker.hpp>
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
} // namespace

std::vector<VkPhysicalDevice> vkrndr::query_physical_devices(
    VkInstance instance)
{
    uint32_t count{};
    vkEnumeratePhysicalDevices(instance, &count, nullptr);

    std::vector<VkPhysicalDevice> rv{count};
    vkEnumeratePhysicalDevices(instance, &count, rv.data());

    return rv;
}

std::vector<vkrndr::queue_family_t>
vkrndr::query_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    uint32_t count{};
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);

    std::vector<VkQueueFamilyProperties> family_properties{count};
    vkGetPhysicalDeviceQueueFamilyProperties(device,
        &count,
        family_properties.data());

    std::vector<queue_family_t> rv;
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

vkrndr::device_t vkrndr::create_device(context_t const& context,
    device_create_info_t const& create_info,
    VkSurfaceKHR const surface)
{
    device_t rv;
    rv.physical = create_info.device;
    rv.max_msaa_samples = max_usable_sample_count(rv.physical);

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

    VkDeviceCreateInfo ci{.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = create_info.chain,
        .queueCreateInfoCount = count_cast(queue_create_infos.size()),
        .pQueueCreateInfos = queue_create_infos.data(),
        .enabledExtensionCount = count_cast(create_info.extensions.size()),
        .ppEnabledExtensionNames = create_info.extensions.data()};

    check_result(vkCreateDevice(create_info.device, &ci, nullptr, &rv.logical));
    boost::scope::scope_fail const rollback{[&rv]() { destroy(&rv); }};

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

    rv.execution_ports.reserve(queue_create_infos.size());
    for (auto const& family : create_info.queues)
    {
        rv.execution_ports.emplace_back(rv.logical,
            family.properties.queueFlags,
            family.index,
            0);
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
