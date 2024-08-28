#ifndef VKRNDR_DEVICE_INCLUDED
#define VKRNDR_DEVICE_INCLUDED

#include <vkrndr_execution_port.hpp>

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

        std::vector<execution_port_t> execution_ports;
        execution_port_t* present_queue{nullptr};
        execution_port_t* transfer_queue{nullptr};

        VmaAllocator allocator{VK_NULL_HANDLE};
    };

    device_t create_device(context_t const& context);

    void destroy(device_t* device);

    [[nodiscard]] VkCommandPool create_command_pool(device_t const& device,
        uint32_t queue_family);
} // namespace vkrndr

#endif
