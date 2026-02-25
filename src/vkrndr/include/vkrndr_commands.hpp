#ifndef VKRNDR_COMMANDS_INCLUDED
#define VKRNDR_COMMANDS_INCLUDED

#include <volk.h>

#include <cstdint>
#include <expected>
#include <span>
#include <system_error>

namespace vkrndr
{
    struct device_t;
    class execution_port_t;
} // namespace vkrndr

namespace vkrndr
{
    // Command pool
    [[nodiscard]] std::expected<VkCommandPool, std::error_code>
    create_command_pool(device_t const& device,
        uint32_t family,
        VkCommandPoolCreateFlags flags = 0);

    [[nodiscard]] std::expected<void, std::error_code> allocate_command_buffers(
        device_t const& device,
        VkCommandPool pool,
        bool primary,
        std::span<VkCommandBuffer> buffers);

    void free_command_buffers(device_t const& device,
        VkCommandPool pool,
        std::span<VkCommandBuffer const> const& buffers);

    [[nodiscard]] std::expected<void, std::error_code> reset_command_pool(
        device_t const& device,
        VkCommandPool pool,
        bool release_resources = false);

    void trim_command_pool(device_t const& device, VkCommandPool pool);

    void destroy_command_pool(device_t const& device, VkCommandPool pool);

    // One time submit

    [[nodiscard]] std::expected<void, std::error_code>
    begin_single_time_commands(device_t const& device,
        VkCommandPool pool,
        std::span<VkCommandBuffer> buffers);

    [[nodiscard]] std::expected<void, std::error_code> end_single_time_commands(
        device_t const& device,
        VkCommandPool pool,
        execution_port_t& port,
        std::span<VkCommandBuffer const> const& buffers);

    // Individual operations

    void copy_buffer_to_image(VkCommandBuffer command_buffer,
        VkBuffer buffer,
        VkImage image,
        VkExtent2D extent);

    void copy_buffer_to_buffer(VkCommandBuffer command_buffer,
        VkBuffer source_buffer,
        VkDeviceSize size,
        VkBuffer target_buffer);

    void wait_for_color_attachment_read(VkImage image,
        VkCommandBuffer command_buffer);

    void wait_for_color_attachment_write(VkImage image,
        VkCommandBuffer command_buffer);

    void transition_to_present_layout(VkImage image,
        VkCommandBuffer command_buffer);

    void wait_for_transfer_write(VkImage image,
        VkCommandBuffer command_buffer,
        uint32_t mip_levels);

    void wait_for_transfer_write_completed(VkImage image,
        VkCommandBuffer command_buffer,
        uint32_t mip_levels);

    void generate_mipmaps(device_t const& device,
        VkImage image,
        VkCommandBuffer command_buffer,
        VkFormat format,
        VkExtent2D extent,
        uint32_t mip_levels,
        uint32_t layers = 1);
} // namespace vkrndr

#endif
