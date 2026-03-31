#include <vkrndr_commands.hpp>

#include <vkrndr_device.hpp>
#include <vkrndr_execution_port.hpp>

#include <catch2/catch_test_macros.hpp>

#include <volk.h>

#include <expected>
#include <system_error>
#include <vector>

#include <global_library_handle.hpp>

TEST_CASE("VkCommandPool utilities", "[vkrndr][commands][gpu]")
{
    vkrndr::execution_port_t const& port{
        test::minimal_device->execution_ports.front()};

    std::expected<VkCommandPool, std::system_error> const create_result{
        create_command_pool(*test::minimal_device, port.queue_family())};
    REQUIRE(create_result);

    destroy_command_pool(*test::minimal_device, *create_result);
}
