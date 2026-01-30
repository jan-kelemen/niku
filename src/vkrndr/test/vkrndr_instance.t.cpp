#include <vkrndr_instance.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_contains.hpp>

#include <volk.h>

#include <array>

#include <global_library_handle.hpp>

TEST_CASE("Optional instance extension is added when available",
    "[vkrndr][instance]")
{
    static constexpr auto extension{
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME};

    // This should pretty much be present on any Vulkan 1.1 capable GPU
    if (!vkrndr::is_instance_extension_available(extension))
    {
        SKIP(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
            " isn't supported");
    }

    static constexpr auto extensions{std::to_array<char const*>({extension})};
    auto instance_result{
        vkrndr::create_instance({.optional_extensions = extensions})};
    REQUIRE(instance_result);

    auto const& instance{*instance_result};
    Catch::Matchers::Contains(instance->extensions, extension);
}
