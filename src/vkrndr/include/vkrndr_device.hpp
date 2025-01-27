#ifndef VKRNDR_DEVICE_INCLUDED
#define VKRNDR_DEVICE_INCLUDED

#include <vkrndr_execution_port.hpp>

#include <vma_impl.hpp>

#include <volk.h>

#include <cstdint>
#include <span>
#include <vector>

namespace vkrndr
{
    struct context_t;
} // namespace vkrndr

namespace vkrndr
{
    [[nodiscard]] std::vector<VkPhysicalDevice> query_physical_devices(
        VkInstance instance);

    struct [[nodiscard]] queue_family_t final
    {
        uint32_t index;
        VkQueueFamilyProperties properties;
        bool supports_present;
    };

    [[nodiscard]] std::vector<queue_family_t> query_queue_families(
        VkPhysicalDevice device,
        VkSurfaceKHR surface = VK_NULL_HANDLE);

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

    struct [[nodiscard]] device_create_info_t final
    {
        void const* chain{};

        VkPhysicalDevice device;
        std::span<char const* const> extensions;
        std::span<queue_family_t const> queues;
    };

    device_t create_device(context_t const& context,
        device_create_info_t const& create_info);

    void destroy(device_t* device);
} // namespace vkrndr

#endif
