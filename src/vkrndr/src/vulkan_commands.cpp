#include <vulkan_commands.hpp>

#include <vulkan_device.hpp>
#include <vulkan_utility.hpp>

#include <cppext_numeric.hpp>

#include <algorithm>
#include <cassert>
#include <stdexcept>

void vkrndr::transition_image(VkImage const image,
    VkCommandBuffer const command_buffer,
    VkImageLayout const old_layout,
    VkPipelineStageFlags2 const src_stage_mask,
    VkAccessFlags2 const src_access_mask,
    VkImageLayout const new_layout,
    VkPipelineStageFlags2 const dst_stage_mask,
    VkAccessFlags2 const dst_access_mask,
    uint32_t const mip_levels)
{
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.oldLayout = old_layout;
    barrier.srcStageMask = src_stage_mask;
    barrier.srcAccessMask = src_access_mask;
    barrier.newLayout = new_layout;
    barrier.dstStageMask = dst_stage_mask;
    barrier.dstAccessMask = dst_access_mask;
    barrier.image = image;
    barrier.subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = mip_levels,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };

    VkDependencyInfo dependency{};
    dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency.imageMemoryBarrierCount = 1;
    dependency.pImageMemoryBarriers = &barrier;

    vkCmdPipelineBarrier2(command_buffer, &dependency);
}

void vkrndr::create_command_buffers(vulkan_device const& device,
    VkCommandPool const command_pool,
    uint32_t const count,
    VkCommandBufferLevel const level,
    std::span<VkCommandBuffer> const buffers)
{
    assert(buffers.size() >= count);

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = command_pool;
    alloc_info.level = level;
    alloc_info.commandBufferCount = count;

    check_result(
        vkAllocateCommandBuffers(device.logical, &alloc_info, buffers.data()));
}

void vkrndr::begin_single_time_commands(vulkan_device const& device,
    VkCommandPool const command_pool,
    uint32_t const count,
    std::span<VkCommandBuffer> const buffers)
{
    create_command_buffers(device,
        command_pool,
        count,
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        buffers);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    for (uint32_t i{}; i != count; ++i)
    {
        check_result(vkBeginCommandBuffer(buffers[i], &begin_info));
    }
}

void vkrndr::end_single_time_commands(vulkan_device const& device,
    VkQueue const queue,
    std::span<VkCommandBuffer> const command_buffers,
    VkCommandPool const command_pool)
{
    for (VkCommandBuffer buffer : command_buffers)
    {
        check_result(vkEndCommandBuffer(buffer));
    }

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = count_cast(command_buffers.size());
    submit_info.pCommandBuffers = command_buffers.data();

    // TODO-JK: Use fence instead of waiting to for idle
    check_result(vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE));
    check_result(vkQueueWaitIdle(queue));

    vkFreeCommandBuffers(device.logical,
        command_pool,
        submit_info.commandBufferCount,
        submit_info.pCommandBuffers);
}

void vkrndr::copy_buffer_to_image(VkCommandBuffer const command_buffer,
    VkBuffer const buffer,
    VkImage const image,
    VkExtent2D const extent)
{
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = {0, 0, 0};
    region.imageExtent = {extent.width, extent.height, 1};

    vkCmdCopyBufferToImage(command_buffer,
        buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region);
}

void vkrndr::copy_buffer_to_buffer(VkCommandBuffer const command_buffer,
    VkBuffer const source_buffer,
    VkDeviceSize const size,
    VkBuffer const target_buffer)
{
    VkBufferCopy const region{.srcOffset = 0, .dstOffset = 0, .size = size};

    vkCmdCopyBuffer(command_buffer, source_buffer, target_buffer, 1, &region);
}

void vkrndr::wait_for_color_attachment_read(VkImage const image,
    VkCommandBuffer command_buffer)
{
    transition_image(image,
        command_buffer,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
        1);
}

void vkrndr::wait_for_color_attachment_write(VkImage const image,
    VkCommandBuffer command_buffer)
{
    // Wait for COLOR_ATTACHMENT_OUTPUT instead of TOP/BOTTOM of pipe
    // to allow for acquisition of the image to finish
    //
    // https://github.com/KhronosGroup/Vulkan-ValidationLayers/issues/7193#issuecomment-1875960974
    transition_image(image,
        command_buffer,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_NONE,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        1);
}

void vkrndr::transition_to_present_layout(VkImage const image,
    VkCommandBuffer command_buffer)
{
    transition_image(image,
        command_buffer,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
        VK_ACCESS_2_NONE,
        1);
}

void vkrndr::wait_for_transfer_write(VkImage image,
    VkCommandBuffer command_buffer,
    uint32_t const mip_levels)
{
    transition_image(image,
        command_buffer,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
        VK_ACCESS_2_NONE,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT,
        mip_levels);
}

void vkrndr::wait_for_transfer_write_completed(VkImage image,
    VkCommandBuffer command_buffer,
    uint32_t const mip_levels)
{
    transition_image(image,
        command_buffer,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
        VK_ACCESS_2_NONE,
        mip_levels);
}

void vkrndr::generate_mipmaps(vulkan_device const& device,
    VkImage image,
    VkCommandBuffer command_buffer,
    VkFormat const format,
    VkExtent2D const extent,
    uint32_t const mip_levels)
{
    VkFormatProperties properties;
    vkGetPhysicalDeviceFormatProperties(device.physical, format, &properties);
    if (!(properties.optimalTilingFeatures &
            VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
    {
        throw std::runtime_error{
            "texture image format does not support linear blitting!"};
    }

    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    VkDependencyInfo dependency{};
    dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency.imageMemoryBarrierCount = 1;
    dependency.pImageMemoryBarriers = &barrier;

    auto mip_width{cppext::narrow<int32_t>(extent.width)};
    auto mip_height{cppext::narrow<int32_t>(extent.height)};
    for (uint32_t i{1}; i != mip_levels; ++i)
    {
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.subresourceRange.baseMipLevel = i - 1;

        vkCmdPipelineBarrier2(command_buffer, &dependency);

        VkImageBlit blit{};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {mip_width, mip_height, 1};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {mip_width > 1 ? mip_width / 2 : 1,
            mip_height > 1 ? mip_height / 2 : 1,
            1};
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(command_buffer,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &blit,
            VK_FILTER_LINEAR);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier2(command_buffer, &dependency);

        mip_width = std::max(1, mip_width / 2);
        mip_height = std::max(1, mip_height / 2);
    }

    barrier.subresourceRange.baseMipLevel = mip_levels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier2(command_buffer, &dependency);
}
