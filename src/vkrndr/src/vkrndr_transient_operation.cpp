#include <vkrndr_transient_operation.hpp>

#include <vkrndr_commands.hpp>

#include <cppext_container.hpp>

#include <span>
#include <utility>

vkrndr::transient_operation_t::transient_operation_t(device_t& device,
    execution_port_t& port,
    VkCommandPool pool)
    : device_{&device}
    , port_{&port}
    , pool_{pool}
{
    begin_single_time_commands(*device_,
        pool_,
        1,
        cppext::as_span(command_buffer_));
}

vkrndr::transient_operation_t::transient_operation_t(
    transient_operation_t&& other) noexcept
    : device_{std::exchange(other.device_, nullptr)}
    , port_{std::exchange(other.port_, nullptr)}
    , pool_{std::exchange(other.pool_, nullptr)}
    , command_buffer_{std::exchange(command_buffer_, VK_NULL_HANDLE)}
{
}

vkrndr::transient_operation_t::~transient_operation_t()
{
    if (command_buffer_ != VK_NULL_HANDLE)
    {
        end_single_time_commands(*device_,
            pool_,
            *port_,
            cppext::as_span(command_buffer_));
    }
}
