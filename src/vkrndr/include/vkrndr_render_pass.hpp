#ifndef VKRNDR_RENDER_PASS_INCLUDED
#define VKRNDR_RENDER_PASS_INCLUDED

#include <vulkan/vulkan_core.h>

#include <optional>
#include <vector>

namespace vkrndr
{
    struct [[nodiscard]] render_pass_guard final
    {
    public:
        explicit render_pass_guard(VkCommandBuffer command_buffer);

        render_pass_guard(render_pass_guard const&) = delete;

        render_pass_guard(render_pass_guard&& other) noexcept;

    public:
        ~render_pass_guard();

    public:
        render_pass_guard& operator=(render_pass_guard const&) = delete;

        render_pass_guard& operator=(
            render_pass_guard&& other) noexcept = delete;

    private:
        VkCommandBuffer command_buffer_;
    };

    class [[nodiscard]] render_pass final
    {
    public:
        render_pass() = default;

        render_pass(render_pass const&) = default;

        render_pass(render_pass&&) noexcept = default;

    public:
        ~render_pass() = default;

    public:
        [[nodiscard]] render_pass_guard begin(VkCommandBuffer command_buffer,
            VkRect2D const& render_area) const;

        render_pass& with_color_attachment(VkAttachmentLoadOp load_operation,
            VkAttachmentStoreOp store_operation,
            VkImageView color_image,
            std::optional<VkClearValue> const& clear_color = {},
            std::optional<VkImageView> const& intermediate_image = {});

        render_pass& with_depth_attachment(VkAttachmentLoadOp load_operation,
            VkAttachmentStoreOp store_operation,
            VkImageView depth_image,
            std::optional<VkClearValue> const& clear_value = {});

        render_pass& with_stencil_attachment(VkAttachmentLoadOp load_operation,
            VkAttachmentStoreOp store_operation,
            VkImageView stencil_image,
            std::optional<VkClearValue> const& clear_value = {});

    public:
        render_pass& operator=(render_pass const&) = default;

        render_pass& operator=(render_pass&&) noexcept = default;

    private:
        std::vector<VkRenderingAttachmentInfo> color_attachments_;
        std::optional<VkRenderingAttachmentInfo> depth_attachment_;
        std::optional<VkRenderingAttachmentInfo> stencil_attachment_;
    };
} // namespace vkrndr

#endif
