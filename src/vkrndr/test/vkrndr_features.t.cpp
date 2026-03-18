#include <vkrndr_features.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <volk.h>

#include <cstdint>

TEST_CASE("Feature chain respects max Vulkan API version", "[vkrndr][features]")
{
    vkrndr::feature_chain_t chain;

    uint32_t const api_version{GENERATE(VK_API_VERSION_1_0,
        VK_API_VERSION_1_1,
        VK_API_VERSION_1_2,
        VK_API_VERSION_1_3)};

    link_required_feature_chain(chain, api_version);
    switch (api_version)
    {
    case VK_API_VERSION_1_0:
        CHECK_FALSE(chain.device_10_features.pNext);
        break;
    case VK_API_VERSION_1_1:
        CHECK_FALSE(chain.device_11_features.pNext);
        break;
    case VK_API_VERSION_1_2:
        CHECK_FALSE(chain.device_12_features.pNext);
        break;
    case VK_API_VERSION_1_3:
        CHECK_FALSE(chain.device_13_features.pNext);
        break;
    default:
        FAIL("Unexpected value for Vulkan API version");
    }
}
