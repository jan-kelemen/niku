#ifndef VKRNDR_DEVICE_INCLUDED
#define VKRNDR_DEVICE_INCLUDED

#include <vkrndr_execution_port.hpp>
#include <vkrndr_features.hpp>

#include <vma_impl.hpp>

#include <volk.h>

#include <cstdint>
#include <functional>
#include <set>
#include <span>
#include <string>
#include <vector>

namespace vkrndr
{
    struct instance_t;
} // namespace vkrndr

namespace vkrndr
{
    struct [[nodiscard]] device_t final
    {
        VkPhysicalDevice physical{VK_NULL_HANDLE};
        VkDevice logical{VK_NULL_HANDLE};

        std::set<std::string, std::less<>> extensions;

        VkSampleCountFlagBits max_msaa_samples{VK_SAMPLE_COUNT_1_BIT};

        std::vector<execution_port_t> execution_ports;
        execution_port_t* present_queue{nullptr};
        execution_port_t* transfer_queue{nullptr};

        VmaAllocator allocator{VK_NULL_HANDLE};
    };

    [[nodiscard]] bool is_device_extension_enabled(
        char const* const extension_name,
        device_t const& device);

    struct [[nodiscard]] device_create_info_t final
    {
        void const* chain{};

        VkPhysicalDevice device;
        std::span<char const* const> extensions;
        std::span<queue_family_t const> queues;
    };

    device_t create_device(instance_t const& instance,
        device_create_info_t const& create_info);

    void destroy(device_t* device);
} // namespace vkrndr

#endif
