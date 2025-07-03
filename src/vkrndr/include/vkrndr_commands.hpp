#ifndef VKRNDR_COMMANDS_INCLUDED
#define VKRNDR_COMMANDS_INCLUDED

#include <volk.h>

#include <cstdint>
#include <expected>
#include <span>

namespace vkrndr
{
    struct device_t;
    class execution_port_t;
} // namespace vkrndr

namespace vkrndr
{
    // Command pool
    std::expected<VkCommandPool, VkResult> create_command_pool(
        device_t const& device,
        uint32_t family,
        VkCommandPoolCreateFlags flags = 0);

    VkResult allocate_command_buffers(device_t const& device,
        VkCommandPool pool,
        bool primary,
        uint32_t count,
        std::span<VkCommandBuffer> buffers);

    void free_command_buffers(device_t const& device,
        VkCommandPool pool,
        std::span<VkCommandBuffer const> const& buffers);

    VkResult reset_command_pool(device_t const& device,
        VkCommandPool pool,
        bool release_resources = false);

    void trim_command_pool(device_t const& device, VkCommandPool pool);

    void destroy_command_pool(device_t const& device, VkCommandPool pool);

    // One time submit

    void begin_single_time_commands(device_t const& device,
        VkCommandPool pool,
        uint32_t count,
        std::span<VkCommandBuffer> buffers);

    void end_single_time_commands(device_t const& device,
        VkCommandPool pool,
        execution_port_t& port,
        std::span<VkCommandBuffer const> const& command_buffers);

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
