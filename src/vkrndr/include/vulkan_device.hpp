#ifndef VKRNDR_VULKAN_DEVICE_INCLUDED
#define VKRNDR_VULKAN_DEVICE_INCLUDED

#include <vulkan_queue.hpp>

#include <vma_impl.hpp>

#include <vulkan/vulkan_core.h>

#include <vector>

namespace vkrndr
{
    struct vulkan_context;
} // namespace vkrndr

namespace vkrndr
{
    struct [[nodiscard]] vulkan_device final
    {
        VkPhysicalDevice physical{VK_NULL_HANDLE};
        VkDevice logical{VK_NULL_HANDLE};
        VkSampleCountFlagBits max_msaa_samples{VK_SAMPLE_COUNT_1_BIT};
        std::vector<vulkan_queue> queues;
        vulkan_queue* transfer_queue{nullptr};
        vulkan_queue* present_queue{nullptr};
        VmaAllocator allocator{VK_NULL_HANDLE};
    };

    vulkan_device create_device(vulkan_context const& context);

    void destroy(vulkan_device* device);
} // namespace vkrndr

#endif // !VKRNDR_VULKAN_DEVICE_INCLUDED
