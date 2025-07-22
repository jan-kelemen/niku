#ifndef VKRNDR_DEVICE_INCLUDED
#define VKRNDR_DEVICE_INCLUDED

#include <vkrndr_execution_port.hpp>
#include <vkrndr_features.hpp>

#include <boost/smart_ptr/intrusive_ptr.hpp>
#include <boost/smart_ptr/intrusive_ref_counter.hpp>

#include <vma_impl.hpp>

#include <volk.h>

#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <system_error>
#include <vector>

namespace vkrndr
{
    struct instance_t;
} // namespace vkrndr

namespace vkrndr
{
    struct [[nodiscard]] device_t final
        : public boost::intrusive_ref_counter<device_t>
    {
        VkPhysicalDevice physical_device{VK_NULL_HANDLE};
        VkDevice logical_device{VK_NULL_HANDLE};

        std::set<std::string, std::less<>> extensions;

        VkSampleCountFlagBits max_msaa_samples{VK_SAMPLE_COUNT_1_BIT};

        std::vector<execution_port_t> execution_ports;

        VmaAllocator allocator{VK_NULL_HANDLE};

    public:
        device_t() = default;

        device_t(device_t const&) = delete;

        device_t(device_t&&) noexcept = delete;

    public:
        ~device_t();

    public:
        [[nodiscard]] operator VkPhysicalDevice() const noexcept;

        [[nodiscard]] operator VkDevice() const noexcept;

        device_t& operator=(device_t const&) = delete;

        device_t& operator=(device_t&&) noexcept = delete;
    };

    using device_ptr_t = boost::intrusive_ptr<device_t>;

    inline device_t::operator VkDevice() const noexcept
    {
        return logical_device;
    }

    inline device_t::operator VkPhysicalDevice() const noexcept
    {
        return physical_device;
    }

    [[nodiscard]] bool is_device_extension_enabled(char const* extension_name,
        device_t const& device);

    [[nodiscard]] std::expected<device_ptr_t, std::error_code> create_device(
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
