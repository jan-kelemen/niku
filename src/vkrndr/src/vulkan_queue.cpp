#include <vulkan_queue.hpp>

#include <vulkan_device.hpp>
#include <vulkan_utility.hpp>

#include <vector>

vkrndr::vulkan_queue vkrndr::create_queue(vulkan_device const* const device,
    uint32_t const family,
    uint32_t const queue_index)
{
    vulkan_queue rv;

    rv.family = family;
    vkGetDeviceQueue(device->logical, family, queue_index, &rv.queue);

    return rv;
}

vkrndr::queue_families vkrndr::find_queue_families(
    VkPhysicalDevice const physical_device,
    VkSurfaceKHR const surface)
{
    queue_families indices;

    uint32_t count{};
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, nullptr);

    std::vector<VkQueueFamilyProperties> queue_families{count};
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device,
        &count,
        queue_families.data());

    uint32_t index{};
    for (auto const& queue_family : queue_families)
    {
        bool const has_transfer{
            (queue_family.queueFlags & VK_QUEUE_TRANSFER_BIT) != 0};
        auto const has_graphics{
            (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0};
        if (has_transfer && !has_graphics)
        {
            indices.dedicated_transfer_family = index;
            break;
        }
        ++index;
    }

    index = 0;
    for (auto const& queue_family : queue_families)
    {
        if ((queue_family.queueFlags &
                (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT)) != 0)
        {
            VkBool32 present_support{VK_FALSE};
            check_result(vkGetPhysicalDeviceSurfaceSupportKHR(physical_device,
                index,
                surface,
                &present_support));
            if (present_support == VK_TRUE)
            {
                indices.present_family = index;
                break;
            }
        }
        ++index;
    }

    return indices;
}

VkCommandPool vkrndr::create_command_pool(vulkan_device const& device,
    uint32_t const queue_family)
{
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    pool_info.queueFamilyIndex = queue_family;

    VkCommandPool rv; // NOLINT
    check_result(vkCreateCommandPool(device.logical, &pool_info, nullptr, &rv));

    return rv;
}
