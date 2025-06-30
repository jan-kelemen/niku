#include <vkrndr_commands.hpp>

#include <vkrndr_device.hpp>
#include <vkrndr_execution_port.hpp>
#include <vkrndr_synchronization.hpp>
#include <vkrndr_utility.hpp>

#include <cppext_container.hpp>
#include <cppext_numeric.hpp>

#include <vulkan/utility/vk_struct_helper.hpp>

#include <algorithm>
#include <span>
#include <stdexcept>

namespace
{
    void transition_image(VkImage const image,
        VkCommandBuffer const command_buffer,
        VkImageLayout const old_layout,
        VkPipelineStageFlags2 const src_stage_mask,
        VkAccessFlags2 const src_access_mask,
        VkImageLayout const new_layout,
        VkPipelineStageFlags2 const dst_stage_mask,
        VkAccessFlags2 const dst_access_mask,
        uint32_t const mip_levels = 1,
        uint32_t const layers = 1)
    {
        auto const barrier{vkrndr::with_layout(
            vkrndr::with_access(
                vkrndr::on_stage(
                    vkrndr::image_barrier(image,
                        vkrndr::whole_resource(VK_IMAGE_ASPECT_COLOR_BIT,
                            mip_levels,
                            layers)),
                    src_stage_mask,
                    dst_stage_mask),
                src_access_mask,
                dst_access_mask),
            old_layout,
            new_layout)};

        vkrndr::wait_for(command_buffer, {}, {}, cppext::as_span(barrier));
    }
} // namespace

std::expected<VkCommandPool, VkResult> vkrndr::create_command_pool(
    device_t const& device,
    uint32_t const family,
    VkCommandPoolCreateFlags const flags)
{
    VkCommandPoolCreateInfo const create_info{
        .sType = vku::GetSType<VkCommandPoolCreateInfo>(),
        .flags = flags,
        .queueFamilyIndex = family,
    };

    VkCommandPool rv{};
    if (auto const result{
            vkCreateCommandPool(device.logical, &create_info, nullptr, &rv)};
        result != VK_SUCCESS)
    {
        return std::unexpected{result};
    }

    return rv;
}

VkResult vkrndr::allocate_command_buffers(device_t const& device,
    VkCommandPool pool,
    bool primary,
    uint32_t count,
    std::span<VkCommandBuffer> buffers)
{
    assert(count <= buffers.size());

    VkCommandBufferAllocateInfo const alloc_info{
        .sType = vku::GetSType<VkCommandBufferAllocateInfo>(),
        .commandPool = pool,
        .level = primary ? VK_COMMAND_BUFFER_LEVEL_PRIMARY
                         : VK_COMMAND_BUFFER_LEVEL_SECONDARY,
        .commandBufferCount = count,
    };

    return vkAllocateCommandBuffers(device.logical,
        &alloc_info,
        buffers.data());
}

void vkrndr::free_command_buffers(device_t const& device,
    VkCommandPool pool,
    std::span<VkCommandBuffer const> const& buffers)
{
    vkFreeCommandBuffers(device.logical,
        pool,
        count_cast(buffers.size()),
        buffers.data());
}

VkResult vkrndr::reset_command_pool(device_t const& device,
    VkCommandPool pool,
    bool release_resources)
{
    return vkResetCommandPool(device.logical,
        pool,
        release_resources ? VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT : 0);
}

void vkrndr::trim_command_pool(device_t const& device, VkCommandPool pool)
{
    vkTrimCommandPool(device.logical, pool, 0);
}

void vkrndr::destroy_command_pool(device_t const& device, VkCommandPool pool)
{
    vkDestroyCommandPool(device.logical, pool, nullptr);
}

void vkrndr::begin_single_time_commands(device_t const& device,
    VkCommandPool pool,
    uint32_t count,
    std::span<VkCommandBuffer> buffers)
{
    check_result(allocate_command_buffers(device, pool, true, count, buffers));

    VkCommandBufferBeginInfo const begin_info{
        .sType = vku::GetSType<VkCommandBufferBeginInfo>(),
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    for (uint32_t i{}; i != count; ++i)
    {
        check_result(vkBeginCommandBuffer(buffers[i], &begin_info));
    }
}

void vkrndr::end_single_time_commands(device_t const& device,
    VkCommandPool pool,
    execution_port_t& port,
    std::span<VkCommandBuffer const> const& command_buffers)
{
    for (VkCommandBuffer buffer : command_buffers)
    {
        check_result(vkEndCommandBuffer(buffer));
    }

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = count_cast(command_buffers.size());
    submit_info.pCommandBuffers = command_buffers.data();

    port.submit(cppext::as_span(submit_info));
    port.wait_idle();

    free_command_buffers(device, pool, command_buffers);
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

void vkrndr::generate_mipmaps(device_t const& device,
    VkImage image,
    VkCommandBuffer command_buffer,
    VkFormat const format,
    VkExtent2D const extent,
    uint32_t const mip_levels,
    uint32_t const layers)
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
    barrier.subresourceRange.layerCount = layers;
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
        barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
        barrier.subresourceRange.baseMipLevel = i - 1;

        vkCmdPipelineBarrier2(command_buffer, &dependency);

        VkImageBlit blit{};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {mip_width, mip_height, 1};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = layers;
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {mip_width > 1 ? mip_width / 2 : 1,
            mip_height > 1 ? mip_height / 2 : 1,
            1};
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = layers;

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
        barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

        vkCmdPipelineBarrier2(command_buffer, &dependency);

        mip_width = std::max(1, mip_width / 2);
        mip_height = std::max(1, mip_height / 2);
    }

    barrier.subresourceRange.baseMipLevel = mip_levels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    vkCmdPipelineBarrier2(command_buffer, &dependency);
}
