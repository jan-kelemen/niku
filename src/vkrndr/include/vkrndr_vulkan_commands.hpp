#ifndef VKRNDR_VULKAN_COMMANDS_INCLUDED
#define VKRNDR_VULKAN_COMMANDS_INCLUDED

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <span>

namespace vkrndr
{
    struct vulkan_device;
} // namespace vkrndr

namespace vkrndr
{
    void transition_image(VkImage image,
        VkCommandBuffer command_buffer,
        VkImageLayout old_layout,
        VkPipelineStageFlags2 src_stage_mask,
        VkAccessFlags2 src_access_mask,
        VkImageLayout new_layout,
        VkPipelineStageFlags2 dst_stage_mask,
        VkAccessFlags2 dst_access_mask,
        uint32_t mip_levels);

    void create_command_buffers(vkrndr::vulkan_device const& device,
        VkCommandPool command_pool,
        uint32_t count,
        VkCommandBufferLevel level,
        std::span<VkCommandBuffer> buffers);

    void begin_single_time_commands(vulkan_device const& device,
        VkCommandPool command_pool,
        uint32_t count,
        std::span<VkCommandBuffer> buffers);

    void end_single_time_commands(vulkan_device const& device,
        VkQueue queue,
        std::span<VkCommandBuffer> command_buffers,
        VkCommandPool command_pool);

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

    void generate_mipmaps(vulkan_device const& device,
        VkImage image,
        VkCommandBuffer command_buffer,
        VkFormat format,
        VkExtent2D extent,
        uint32_t mip_levels);

} // namespace vkrndr

#endif // !VKRNDR_VULKAN_COMMANDS_INCLUDED
