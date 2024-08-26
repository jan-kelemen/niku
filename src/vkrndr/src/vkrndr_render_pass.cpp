#include <vkrndr_render_pass.hpp>

#include <vkrndr_vulkan_utility.hpp>

#include <cassert>
#include <utility>

vkrndr::render_pass_guard::render_pass_guard(VkCommandBuffer command_buffer)
    : command_buffer_{command_buffer}
{
}

vkrndr::render_pass_guard::render_pass_guard(render_pass_guard&& other) noexcept
    : command_buffer_{std::exchange(other.command_buffer_, VK_NULL_HANDLE)}
{
}

vkrndr::render_pass_guard::~render_pass_guard()
{
    if (command_buffer_ != VK_NULL_HANDLE)
    {
        vkCmdEndRendering(command_buffer_);
    }
}

vkrndr::render_pass_guard vkrndr::render_pass::begin(
    VkCommandBuffer command_buffer,
    VkRect2D const& render_area) const
{
    VkRenderingInfo render_info{};
    render_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    render_info.renderArea = render_area;
    render_info.layerCount = 1;
    render_info.colorAttachmentCount = count_cast(color_attachments_.size());
    render_info.pColorAttachments = color_attachments_.data();

    if (depth_attachment_)
    {
        render_info.pDepthAttachment = &depth_attachment_.value();
    }

    if (stencil_attachment_)
    {
        render_info.pStencilAttachment = &stencil_attachment_.value();
    }

    vkCmdBeginRendering(command_buffer, &render_info);

    return render_pass_guard{command_buffer};
}

vkrndr::render_pass& vkrndr::render_pass::with_color_attachment(
    VkAttachmentLoadOp load_operation,
    VkAttachmentStoreOp store_operation,
    VkImageView color_image,
    std::optional<VkClearValue> const& clear_color,
    std::optional<VkImageView> const& intermediate_image)
{
    VkRenderingAttachmentInfo color_attachment_info{};

    color_attachment_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color_attachment_info.imageLayout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_attachment_info.loadOp = load_operation;
    color_attachment_info.storeOp = store_operation;
    if (clear_color)
    {
        assert(load_operation == VK_ATTACHMENT_LOAD_OP_CLEAR);
        color_attachment_info.clearValue = *clear_color;
    }

    if (intermediate_image)
    {
        color_attachment_info.imageView = *intermediate_image;
        color_attachment_info.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
        color_attachment_info.resolveImageView = color_image;
        color_attachment_info.resolveImageLayout =
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    else
    {
        color_attachment_info.imageView = color_image;
    }

    color_attachments_.push_back(color_attachment_info);

    return *this;
}

vkrndr::render_pass& vkrndr::render_pass::with_depth_attachment(
    VkAttachmentLoadOp load_operation,
    VkAttachmentStoreOp store_operation,
    VkImageView depth_image,
    std::optional<VkClearValue> const& clear_value)
{
    VkRenderingAttachmentInfo depth_attachment_info{};

    depth_attachment_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depth_attachment_info.pNext = nullptr;
    depth_attachment_info.imageView = depth_image;
    depth_attachment_info.imageLayout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depth_attachment_info.resolveMode = VK_RESOLVE_MODE_NONE;
    depth_attachment_info.resolveImageView = VK_NULL_HANDLE;
    depth_attachment_info.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment_info.loadOp = load_operation;
    depth_attachment_info.storeOp = store_operation;
    if (clear_value)
    {
        assert(load_operation == VK_ATTACHMENT_LOAD_OP_CLEAR);
        depth_attachment_info.clearValue = *clear_value;
    }

    depth_attachment_ = depth_attachment_info;

    return *this;
}

vkrndr::render_pass& vkrndr::render_pass::with_stencil_attachment(
    VkAttachmentLoadOp load_operation,
    VkAttachmentStoreOp store_operation,
    VkImageView stencil_image,
    std::optional<VkClearValue> const& clear_value)
{
    VkRenderingAttachmentInfo stencil_attachment_info{};

    stencil_attachment_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    stencil_attachment_info.pNext = nullptr;
    stencil_attachment_info.imageView = stencil_image;
    stencil_attachment_info.imageLayout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    stencil_attachment_info.resolveMode = VK_RESOLVE_MODE_NONE;
    stencil_attachment_info.resolveImageView = VK_NULL_HANDLE;
    stencil_attachment_info.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    stencil_attachment_info.loadOp = load_operation;
    stencil_attachment_info.storeOp = store_operation;
    if (clear_value)
    {
        assert(load_operation == VK_ATTACHMENT_LOAD_OP_CLEAR);
        stencil_attachment_info.clearValue = *clear_value;
    }

    stencil_attachment_ = stencil_attachment_info;

    return *this;
}
