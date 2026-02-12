#include <catch2/catch_config.hpp>
#include <catch2/catch_session.hpp>

#include <global_library_handle.hpp>

#include <vkrndr_device.hpp>
#include <vkrndr_features.hpp>
#include <vkrndr_instance.hpp>
#include <vkrndr_library_handle.hpp>

#include <volk.h>

#include <cstdio>
#include <cstdlib>
#include <expected>
#include <optional>
#include <print>
#include <system_error>

// IWYU pragma: no_include <boost/smart_ptr/intrusive_ref_counter.hpp>
// IWYU pragma: no_include <span>
// IWYU pragma: no_include <string>
// IWYU pragma: no_include <vector>

namespace test
{
    // NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
    vkrndr::library_handle_ptr_t library;
    vkrndr::instance_ptr_t instance;
    vkrndr::device_ptr_t minimal_device;
    // NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)
} // namespace test

// NOLINTNEXTLINE(bugprone-exception-escape)
int main(int argc, char* argv[])
{
    static constexpr int test_initialization_failed{EXIT_FAILURE};

    if (std::expected<vkrndr::library_handle_ptr_t, std::error_code> lh{
            vkrndr::initialize()})
    {
        test::library = *lh;
    }
    else
    {
        std::print(stderr,
            "Skipping tests, Vulkan can't be initialized (): {}",
            lh.error().value(),
            lh.error().message());
        return test_initialization_failed;
    }

    if (std::expected<vkrndr::instance_ptr_t, std::error_code> in{
            vkrndr::create_instance({})})
    {
        test::instance = *in;
    }
    else
    {
        std::print(stderr,
            "Skipping tests, Vulkan instance can't be initialized (): {}",
            in.error().value(),
            in.error().message());
        return test_initialization_failed;
    }

    if (std::optional<vkrndr::physical_device_features_t> const& pd{
            pick_best_physical_device(*test::instance, {}, VK_NULL_HANDLE)})
    {
        vkrndr::feature_chain_t chain;
        link_required_feature_chain(chain);
        if (std::expected<vkrndr::device_ptr_t, std::error_code> ld{
                create_device(*test::instance, {}, *pd, chain, {})})
        {
            test::minimal_device = *ld;
        }
        else
        {
            std::print(stderr,
                "Skipping tests, minimal Vulkan device can't be initialized (): {}",
                ld.error().value(),
                ld.error().message());
            return test_initialization_failed;
        }
    }
    else
    {
        std::print(stderr,
            "Skipping tests, physical Vulkan device can't be selected");
        return test_initialization_failed;
    }

    Catch::ConfigData const config{.allowZeroTests = true};
    Catch::Session session;
    session.useConfigData(config);

    int rv{session.run(argc, argv)};

    test::minimal_device.reset();
    test::instance.reset();
    test::library.reset();

    return rv;
}
