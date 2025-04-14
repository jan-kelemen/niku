#ifndef VKRNDR_COMMANDS_INCLUDED
#define VKRNDR_COMMANDS_INCLUDED

#include <volk.h>

#include <cstdint>
#include <span>

namespace vkrndr
{
    struct device_t;
    class command_pool_t;
    class execution_port_t;
} // namespace vkrndr

namespace vkrndr
{
    void begin_single_time_commands(command_pool_t& pool,
        uint32_t count,
        std::span<VkCommandBuffer> buffers);

    void end_single_time_commands(command_pool_t& pool,
        execution_port_t& port,
        std::span<VkCommandBuffer const> const& command_buffers);

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
