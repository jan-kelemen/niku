#include <catch2/catch_test_case_info.hpp>
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>

#include <vkrndr_device.hpp>
#include <vkrndr_features.hpp>
#include <vkrndr_instance.hpp>
#include <vkrndr_library_handle.hpp>

#include <volk.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <expected>
#include <optional>
#include <print>
#include <system_error>

#include <global_handles.hpp>
#include <helpers.hpp>

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

namespace
{
    void initialize_global_instances()
    {
        if (std::expected<vkrndr::library_handle_ptr_t, std::error_code> lh{
                vkrndr::initialize()})
        {
            test::library = *lh;
            if (std::expected<vkrndr::instance_ptr_t, std::error_code> in{
                    vkrndr::create_instance({.optional_extensions = {}})})
            {
                test::instance = *in;
                if (std::optional<vkrndr::physical_device_features_t> const& pd{
                        pick_best_physical_device(*test::instance,
                            {},
                            VK_NULL_HANDLE)})
                {
                    vkrndr::queue_family_t const* const queue{
                        find_general_queue(pd->queue_families)};
                    if (!queue)
                    {
                        std::print(
                            "Vulkan device doesn't have a generic queue");
                    }

                    vkrndr::feature_chain_t chain;
                    link_required_feature_chain(chain,
                        std::min(test::instance->api_version,
                            pd->properties.apiVersion));
                    if (std::expected<vkrndr::device_ptr_t, std::error_code> ld{
                            create_device(*test::instance,
                                {},
                                *pd,
                                chain,
                                std::to_array({*queue}))})
                    {
                        test::minimal_device = *ld;
                    }
                    else
                    {
                        std::print(stderr,
                            "Minimal Vulkan device can't be initialized (): {}",
                            ld.error().value(),
                            ld.error().message());
                    }
                }
                else
                {
                    std::print(stderr,
                        "Physical Vulkan device can't be selected");
                }
            }
            else
            {
                std::print(stderr,
                    "Vulkan instance can't be initialized (): {}",
                    in.error().value(),
                    in.error().message());
            }
        }
        else
        {
            std::print(stderr,
                "Vulkan can't be initialized (): {}",
                lh.error().value(),
                lh.error().message());
        }
    }

    class gpu_init_listener_t final : public Catch::EventListenerBase
    {
    public:
        using Catch::EventListenerBase::EventListenerBase;

        void testCaseStarting(Catch::TestCaseInfo const& info) override
        {
            if (!test::instance &&
                std::ranges::contains(info.tags, Catch::Tag("gpu")))
            {
                initialize_global_instances();
            }
        }
    };

    CATCH_REGISTER_LISTENER(gpu_init_listener_t)
} // namespace
