#include <vkrndr_transient_operation.hpp>

#include <vkrndr_commands.hpp>

#include <span>

vkrndr::transient_operation_t::transient_operation_t(execution_port_t& port,
    command_pool_t& pool)
    : port_{&port}
    , pool_{&pool}
{
    begin_single_time_commands(*pool_, 1, std::span{&command_buffer_, 1});
}

vkrndr::transient_operation_t::transient_operation_t(
    transient_operation_t&&) noexcept
    : port_{std::exchange(port_, nullptr)}
    , pool_{std::exchange(pool_, nullptr)}
    , command_buffer_{std::exchange(command_buffer_, VK_NULL_HANDLE)}
{
}

vkrndr::transient_operation_t::~transient_operation_t()
{
    if (command_buffer_ != VK_NULL_HANDLE)
    {
        end_single_time_commands(*pool_,
            *port_,
            std::span{&command_buffer_, 1});
    }
}
