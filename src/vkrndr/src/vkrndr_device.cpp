#include <vkrndr_device.hpp>

#include <vkrndr_execution_port.hpp>
#include <vkrndr_instance.hpp>
#include <vkrndr_utility.hpp>

#include <boost/scope/scope_fail.hpp>

#include <vma_impl.hpp>

#include <volk.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
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

bool vkrndr::is_device_extension_enabled(char const* const extension_name,
    device_t const& device)
{
    return device.extensions.contains(extension_name);
}

vkrndr::device_t vkrndr::create_device(instance_t const& instance,
    device_create_info_t const& create_info)
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

    VkDeviceCreateInfo const ci{.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = create_info.chain,
        .queueCreateInfoCount = count_cast(queue_create_infos.size()),
        .pQueueCreateInfos = queue_create_infos.data(),
        .enabledExtensionCount = count_cast(create_info.extensions.size()),
        .ppEnabledExtensionNames = create_info.extensions.data()};

    check_result(vkCreateDevice(create_info.device, &ci, nullptr, &rv.logical));
    boost::scope::scope_fail const rollback{[&rv]() { destroy(&rv); }};

    std::ranges::copy(create_info.extensions,
        std::inserter(rv.extensions, rv.extensions.begin()));

    volkLoadDevice(rv.logical);

    VmaVulkanFunctions vma_vulkan_functions{
        .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
    };

    VmaAllocatorCreateInfo allocator_info{
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = rv.physical,
        .device = rv.logical,
        .pVulkanFunctions = &vma_vulkan_functions,
        .instance = instance.handle,
        .vulkanApiVersion = VK_API_VERSION_1_3,
    };

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
        device->extensions.clear();
    }
}
