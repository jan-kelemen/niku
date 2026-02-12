#include <vkrndr_device.hpp>

#include <vkrndr_features.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_contains.hpp>

#include <volk.h>

#include <algorithm>
#include <ranges>
#include <vector>

#include <global_library_handle.hpp>

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
        query_available_physical_devices(*test::instance)};
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
    link_required_feature_chain(chain);

    auto device_result{create_device(*test::instance,
        std::to_array<char const* const>({extension}),
        *device_it,
        {})};
    REQUIRE(device_result);

    auto const& device{*device_result};
    REQUIRE(is_device_extension_enabled(extension, *device));
    CHECK_THAT(device->extensions, Catch::Matchers::Contains(extension));
}
