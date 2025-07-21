#ifndef VKRNDR_DEVICE_INCLUDED
#define VKRNDR_DEVICE_INCLUDED

#include <vkrndr_execution_port.hpp>
#include <vkrndr_features.hpp>

#include <vma_impl.hpp>

#include <volk.h>

#include <functional>
#include <memory>
#include <optional>
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

        VmaAllocator allocator{VK_NULL_HANDLE};
    };

    [[nodiscard]] bool is_device_extension_enabled(char const* extension_name,
        device_t const& device);

    [[nodiscard]] std::shared_ptr<device_t> create_device(
        instance_t const& instance,
        std::span<char const* const> const& extensions,
        physical_device_features_t const& features,
        std::span<queue_family_t const> const& queue_families);

    std::optional<physical_device_features_t> pick_best_physical_device(
        instance_t const& instance,
        std::span<char const* const> const& extensions,
        VkSurfaceKHR surface);
} // namespace vkrndr

#endif
