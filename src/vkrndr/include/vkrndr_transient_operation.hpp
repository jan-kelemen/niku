#ifndef VKRNDR_TRANSIENT_OPERATION_INCLUDED
#define VKRNDR_TRANSIENT_OPERATION_INCLUDED

#include <volk.h>

namespace vkrndr
{
    struct device_t;
    class execution_port_t;
} // namespace vkrndr

namespace vkrndr
{
    class [[nodiscard]] transient_operation_t final
    {
    public:
        transient_operation_t(device_t& device,
            execution_port_t& port,
            VkCommandPool pool);

        transient_operation_t(transient_operation_t const&) = delete;

        transient_operation_t(transient_operation_t&& other) noexcept;

    public:
        ~transient_operation_t();

    public:
        [[nodiscard]] constexpr VkCommandBuffer command_buffer() const noexcept;

    public:
        transient_operation_t& operator=(transient_operation_t const&) = delete;

        transient_operation_t& operator=(
            transient_operation_t&&) noexcept = delete;

    private:
        device_t* device_;
        execution_port_t* port_;
        VkCommandPool pool_;
        VkCommandBuffer command_buffer_{VK_NULL_HANDLE};
    };
} // namespace vkrndr

constexpr VkCommandBuffer
vkrndr::transient_operation_t::command_buffer() const noexcept
{
    return command_buffer_;
}

#endif
