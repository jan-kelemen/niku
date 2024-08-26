#ifndef VKRNDR_DEVICE_INCLUDED
#define VKRNDR_DEVICE_INCLUDED

#include <vkrndr_queue.hpp>

#include <vma_impl.hpp>

#include <vulkan/vulkan_core.h>

#include <vector>

namespace vkrndr
{
    struct context_t;
} // namespace vkrndr

namespace vkrndr
{
    struct [[nodiscard]] device_t final
    {
        VkPhysicalDevice physical{VK_NULL_HANDLE};
        VkDevice logical{VK_NULL_HANDLE};
        VkSampleCountFlagBits max_msaa_samples{VK_SAMPLE_COUNT_1_BIT};
        std::vector<queue_t> queues;
        queue_t* transfer_queue{nullptr};
        queue_t* present_queue{nullptr};
        VmaAllocator allocator{VK_NULL_HANDLE};
    };

    device_t create_device(context_t const& context);

    void destroy(device_t* device);
} // namespace vkrndr

#endif
