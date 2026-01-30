#include <catch2/catch_session.hpp>

#include <global_library_handle.hpp>

#include <vkrndr_library_handle.hpp>

#include <print>

vkrndr::library_handle_ptr_t library;

int main(int argc, char* argv[])
{
    int rv{EXIT_FAILURE};

    if (std::expected<vkrndr::library_handle_ptr_t, std::error_code> lh{
            vkrndr::initialize()})
    {
        library = *lh;

        Catch::ConfigData config{.allowZeroTests = true};
        Catch::Session session;
        session.useConfigData(config);

        rv = session.run(argc, argv);
    }
    else
    {
        std::print(stderr,
            "Skipping tests, Vulkan can't be initialized (): {}",
            lh.error().value(),
            lh.error().message());
    }

    return rv;
}
