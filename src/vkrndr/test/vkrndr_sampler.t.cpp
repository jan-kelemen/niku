#include <vkrndr_sampler.hpp>

#include <vkrndr_device.hpp>

#include <boost/hash2/hash_append.hpp>
#include <boost/hash2/md5.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <volk.h>

#include <global_library_handle.hpp>

// IWYU pragma: no_include <boost/hash2/digest.hpp>
// IWYU pragma: no_include <boost/hash2/flavor.hpp>
// IWYU pragma: no_include <optional>

namespace
{
    [[nodiscard]] constexpr auto hash(vkrndr::sampler_properties_t const& p)
    {
        boost::hash2::md5_128 h1;
        boost::hash2::hash_append(h1, {}, p);
        return h1.result();
    }
} // namespace

TEST_CASE("Sampler anisotropy is clamped to maximum supported value by device",
    "[vkrndr][sampler]")
{
    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(*test::minimal_device, &device_properties);

    vkrndr::sampler_properties_t sampler_properties;
    sampler_properties.max_anisotropy =
        device_properties.limits.maxSamplerAnisotropy + 25.0f;

    VkSamplerCreateInfo const create_info{
        as_create_info(*test::minimal_device, sampler_properties)};
    REQUIRE(create_info.anisotropyEnable);
    CHECK(create_info.maxAnisotropy ==
        device_properties.limits.maxSamplerAnisotropy);
}

TEST_CASE("Sampler properties respect hash and equality invariants",
    "[vkrndr][sampler]")
{
    static constexpr vkrndr::sampler_properties_t default_init;
    static constexpr vkrndr::sampler_properties_t min_filter{
        .minification_filter = VK_FILTER_NEAREST,
    };
    static constexpr vkrndr::sampler_properties_t max_filter{
        .magnification_filter = VK_FILTER_NEAREST,
    };
    static constexpr vkrndr::sampler_properties_t mipmap_mode{
        .mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
    };
    static constexpr vkrndr::sampler_properties_t address_mode_u{
        .address_mode_u = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    };
    static constexpr vkrndr::sampler_properties_t address_mode_v{
        .address_mode_v = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    };
    static constexpr vkrndr::sampler_properties_t address_mode_w{
        .address_mode_w = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    };
    static constexpr vkrndr::sampler_properties_t mip_lod_bias{
        .mip_lod_bias = 1.0f,
    };
    static constexpr vkrndr::sampler_properties_t max_anisotropy{
        .max_anisotropy = 2.0f,
    };
    static constexpr vkrndr::sampler_properties_t compare_op{
        .compare_op = VK_COMPARE_OP_NOT_EQUAL,
    };
    static constexpr vkrndr::sampler_properties_t min_lod{
        .min_lod = 1.0f,
    };
    static constexpr vkrndr::sampler_properties_t max_lod{
        .max_lod = 2.0f,
    };
    static constexpr vkrndr::sampler_properties_t border_color{
        .border_color = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
    };
    static constexpr vkrndr::sampler_properties_t unnormalized_coordinates{
        .unnormalized_coordinates = true,
    };

    SECTION("Different values")
    {
        vkrndr::sampler_properties_t const& properties{GENERATE_REF(min_filter,
            max_filter,
            mipmap_mode,
            address_mode_u,
            address_mode_v,
            address_mode_w,
            mip_lod_bias,
            max_anisotropy,
            compare_op,
            min_lod,
            max_lod,
            border_color,
            unnormalized_coordinates)};

        CHECK(default_init != properties);
        CHECK(hash(default_init) != hash(properties));
    }

    SECTION("Same values")
    {
        {
            static constexpr vkrndr::sampler_properties_t value;
            CHECK(default_init == value);
            CHECK(hash(default_init) == hash(value));
        }

        {
            static constexpr vkrndr::sampler_properties_t value{
                .max_lod = 2.0f,
            };
            CHECK(max_lod == value);
            CHECK(hash(max_lod) == hash(value));
        }
    }
}
