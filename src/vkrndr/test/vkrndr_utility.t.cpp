#include <vkrndr_utility.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <volk.h>

#include <array>

TEST_CASE("is_success_result returns true for positive VkResult values",
    "[vkrndr][utility]")
{
    VkResult const value{GENERATE(VK_SUCCESS, VK_TIMEOUT)};
    CHECK(vkrndr::is_success_result(value));
}

TEST_CASE("is_success_result returns false for error VkResult values",
    "[vkrndr][utility]")
{
    VkResult const value{
        GENERATE(VK_ERROR_OUT_OF_DEVICE_MEMORY, VK_ERROR_OUT_OF_HOST_MEMORY)};
    CHECK_FALSE(vkrndr::is_success_result(value));
}

TEST_CASE("check_result propagates passed result", "[vkrndr][utility]")
{
    CHECK(vkrndr::check_result(VK_TIMEOUT) == VK_TIMEOUT);
}

TEST_CASE("count_cast returns container element count", "[vkrndr][utility]")
{
    static constexpr std::array<VkDevice, 1> const devices1{};
    CHECK(vkrndr::count_cast(devices1) == 1u);

    static constexpr std::array<VkDevice, 3> const devices3{};
    CHECK(vkrndr::count_cast(devices3) == 3u);
}

TEST_CASE("to_2d_extent creates extent from width and height",
    "[vkrndr][utility]")
{
    static constexpr VkExtent2D result{vkrndr::to_2d_extent(100u, 50u)};
    CHECK(result.width == 100u);
    CHECK(result.height == 50u);
}

TEST_CASE("to_2d_extent drops depth from higher order extent",
    "[vkrndr][utility]")
{
    static constexpr VkExtent3D initial{100u, 50u, 20u};
    CHECK(initial.width == 100u);
    CHECK(initial.height == 50u);
}

TEST_CASE("to_3d_extent creates extent from width value", "[vkrndr][utility]")
{
    static constexpr VkExtent3D result{vkrndr::to_3d_extent(100u)};
    CHECK(result.width == 100u);
    CHECK(result.height == 1u);
    CHECK(result.depth == 1u);
}

TEST_CASE("to_3d_extent creates extent from width, height, and depth",
    "[vkrndr][utility]")
{
    static constexpr VkExtent3D result{vkrndr::to_3d_extent(100u, 50u, 25u)};
    CHECK(result.width == 100u);
    CHECK(result.height == 50u);
    CHECK(result.depth == 25u);
}

TEST_CASE("to_3d_extent extends 2D extents with depth value",
    "[vkrndr][utility]")
{
    static constexpr VkExtent2D extent2d{100u, 50u};
    static constexpr VkExtent3D result{vkrndr::to_3d_extent(extent2d, 5u)};
    CHECK(result.width == 100u);
    CHECK(result.height == 50u);
    CHECK(result.depth == 5u);
}

TEST_CASE("max_mip_levels calculates correct mip levels", "[vkrndr][utility]")
{
    SECTION("Same dimensions")
    {
        CHECK(vkrndr::max_mip_levels(1, 1) == 1);
        CHECK(vkrndr::max_mip_levels(2, 2) == 2);
        CHECK(vkrndr::max_mip_levels(4, 4) == 3);
        CHECK(vkrndr::max_mip_levels(8, 8) == 4);
        CHECK(vkrndr::max_mip_levels(16, 16) == 5);
        CHECK(vkrndr::max_mip_levels(1024, 1024) == 11);
    }

    SECTION("Different dimensions")
    {
        CHECK(vkrndr::max_mip_levels(1024, 512) == 11);
        CHECK(vkrndr::max_mip_levels(256, 1024) == 11);
        CHECK(vkrndr::max_mip_levels(8192, 8192) == 14);
    }
}

TEST_CASE("supports_flags checks if all bits are set", "[vkrndr][utility]")
{
    SECTION("All flags overlap")
    {
        static constexpr auto const flags{VK_QUEUE_GRAPHICS_BIT |
            VK_QUEUE_TRANSFER_BIT |
            VK_QUEUE_COMPUTE_BIT};
        CHECK(vkrndr::supports_flags(flags,
            VK_QUEUE_GRAPHICS_BIT,
            VK_QUEUE_TRANSFER_BIT,
            VK_QUEUE_COMPUTE_BIT));
    }

    SECTION("Missing flag")
    {
        static constexpr auto const flags{
            VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT};
        CHECK_FALSE(vkrndr::supports_flags(flags,
            VK_QUEUE_GRAPHICS_BIT,
            VK_QUEUE_TRANSFER_BIT,
            VK_QUEUE_COMPUTE_BIT));
    }

    SECTION("No overlap")
    {
        static constexpr auto const flags{VK_QUEUE_GRAPHICS_BIT};
        CHECK_FALSE(vkrndr::supports_flags(flags,
            VK_QUEUE_TRANSFER_BIT,
            VK_QUEUE_COMPUTE_BIT));
    }

    SECTION("No flags")
    {
        static constexpr auto const flags{VK_QUEUE_GRAPHICS_BIT};
        CHECK_FALSE(vkrndr::supports_flags(flags));
    }
}
