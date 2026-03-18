#include <vkrndr_device.hpp>

#include <vkrndr_features.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_contains.hpp>

#include <volk.h>

#include <algorithm>
#include <array>
#include <iterator>
#include <ranges>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <global_library_handle.hpp>
#include <helpers.hpp>

// IWYU pragma: no_include "vkrndr_instance.hpp"
// IWYU pragma: no_include <boost/smart_ptr/intrusive_ptr.hpp>
// IWYU pragma: no_include <boost/smart_ptr/intrusive_ref_counter.hpp>
// IWYU pragma: no_include <expected>
// IWYU pragma: no_include <span>

namespace
{
    template<std::ranges::input_range R>
    [[nodiscard]] constexpr bool has_extension(R&& extensions,
        char const* const extension_name)
    {
        return std::ranges::contains(std::forward<R>(extensions),
            std::string_view{extension_name},
            &VkExtensionProperties::extensionName);
    }
} // namespace

TEST_CASE("Device extension is added when available", "[vkrndr][device]")
{
    static constexpr auto extension{VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME};

    std::vector<vkrndr::physical_device_features_t> const& devices{
        query_available_physical_devices(*test::instance,
            test::instance->api_version)};
    if (devices.empty())
    {
        SKIP("No physical devices available");
    }

    auto const device_it{std::ranges::find_if(
        devices,
        [](auto const& extensions)
        { return has_extension(extensions, extension); },
        &vkrndr::physical_device_features_t::extensions)};
    if (device_it == cend(devices))
    {
        SKIP(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME " isn't supported");
    }

    vkrndr::feature_chain_t chain;
    link_required_feature_chain(chain,
        std::min(test::instance->api_version,
            device_it->properties.apiVersion));

    vkrndr::queue_family_t const* const queue{
        find_general_queue(device_it->queue_families)};
    REQUIRE(queue);
    auto device_result{create_device(*test::instance,
        std::to_array<char const* const>({extension}),
        *device_it,
        std::to_array({*queue}))};
    REQUIRE(device_result);

    auto const& device{*device_result};
    REQUIRE(is_device_extension_enabled(extension, *device));
    CHECK_THAT(device->extensions, Catch::Matchers::Contains(extension));
}

TEST_CASE("Device API version is clamped to maximum supported instance version",
    "[vkrndr][device]")
{
    std::expected<vkrndr::instance_ptr_t, std::system_error> instance_result{
        vkrndr::create_instance({.maximum_vulkan_version = VK_API_VERSION_1_1,
            .optional_extensions = {}})};
    REQUIRE(instance_result);

    auto const& instance{*instance_result};
    std::vector<vkrndr::physical_device_features_t> const& devices{
        query_available_physical_devices(*instance, instance->api_version)};
    if (devices.empty())
    {
        SKIP("No physical devices available");
    }

    auto const device_it{std::ranges::find_if(devices,
        [&instance](vkrndr::physical_device_features_t const& device)
        { return device.properties.apiVersion > instance->api_version; })};
    if (device_it == cend(devices))
    {
        SKIP(
            "No physical device supporting with version higher than Vulkan API 1.1 found");
    }

    vkrndr::feature_chain_t chain;
    link_required_feature_chain(chain,
        std::min(test::instance->api_version,
            device_it->properties.apiVersion));

    vkrndr::queue_family_t const* const queue{
        find_general_queue(device_it->queue_families)};
    REQUIRE(queue);
    auto device_result{
        create_device(*instance, {}, *device_it, std::to_array({*queue}))};
    REQUIRE(device_result);

    auto const& device{*device_result};
    CHECK(device->api_version == instance->api_version);
}
