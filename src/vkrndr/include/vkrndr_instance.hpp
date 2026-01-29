#ifndef VKRNDR_INSTANCE_INCLUDED
#define VKRNDR_INSTANCE_INCLUDED

#include <boost/smart_ptr/intrusive_ptr.hpp>
#include <boost/smart_ptr/intrusive_ref_counter.hpp>

#include <volk.h>

#include <array>
#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <system_error>

namespace vkrndr
{
    constexpr auto instance_optional_extensions{std::to_array({
#if VKRNDR_ENABLE_DEBUG_UTILS
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
        VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
        VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME})};

    struct [[nodiscard]] instance_t final
        : public boost::intrusive_ref_counter<instance_t>
    {
    public:
        VkInstance handle{VK_NULL_HANDLE};

        std::set<std::string, std::less<>> layers;
        std::set<std::string, std::less<>> extensions;

    public:
        instance_t() = default;

        instance_t(instance_t const&) = delete;

        instance_t(instance_t&&) noexcept = delete;

    public:
        ~instance_t();

    public:
        [[nodiscard]] constexpr operator VkInstance() const noexcept;

        instance_t& operator=(instance_t const&) = delete;

        instance_t& operator=(instance_t&&) noexcept = delete;
    };

    using instance_ptr_t = boost::intrusive_ptr<instance_t>;

    struct [[nodiscard]] instance_create_info_t final
    {
        void const* chain{};

        std::optional<uint32_t> maximum_vulkan_version{VK_API_VERSION_1_3};
        std::span<char const* const> extensions;
        std::span<char const* const> optional_extensions{
            instance_optional_extensions};
        std::span<char const* const> layers;

        char const* application_name{};
        uint32_t application_version{};
    };

    [[nodiscard]] std::expected<instance_ptr_t, std::error_code>
    create_instance(instance_create_info_t const& create_info);

    [[nodiscard]] bool is_instance_extension_available(
        char const* extension_name,
        char const* layer_name = nullptr);
} // namespace vkrndr

constexpr vkrndr::instance_t::operator VkInstance() const noexcept
{
    return handle;
}
#endif
