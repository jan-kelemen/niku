#ifndef VKRNDR_RENDER_PASS_INCLUDED
#define VKRNDR_RENDER_PASS_INCLUDED

#include <volk.h>

#include <optional>
#include <vector>

namespace vkrndr
{
    struct [[nodiscard]] render_pass_guard_t final
    {
    public:
        explicit render_pass_guard_t(VkCommandBuffer command_buffer);

        render_pass_guard_t(render_pass_guard_t const&) = delete;

        render_pass_guard_t(render_pass_guard_t&& other) noexcept;

    public:
        ~render_pass_guard_t();

    public:
        render_pass_guard_t& operator=(render_pass_guard_t const&) = delete;

        render_pass_guard_t& operator=(
            render_pass_guard_t&& other) noexcept = delete;

    private:
        VkCommandBuffer command_buffer_;
    };

    class [[nodiscard]] render_pass_t final
    {
    public:
        render_pass_t() = default;

        render_pass_t(render_pass_t const&) = default;

        render_pass_t(render_pass_t&&) noexcept = default;

    public:
        ~render_pass_t() = default;

    public:
        [[nodiscard]] render_pass_guard_t begin(VkCommandBuffer command_buffer,
            VkRect2D const& render_area) const;

        render_pass_t& with_color_attachment(VkAttachmentLoadOp load_operation,
            VkAttachmentStoreOp store_operation,
            VkImageView color_image,
            std::optional<VkClearValue> const& clear_color = {},
            std::optional<VkImageView> const& intermediate_image = {});

        render_pass_t& with_depth_attachment(VkAttachmentLoadOp load_operation,
            VkAttachmentStoreOp store_operation,
            VkImageView depth_image,
            std::optional<VkClearValue> const& clear_value = {});

        render_pass_t& with_stencil_attachment(
            VkAttachmentLoadOp load_operation,
            VkAttachmentStoreOp store_operation,
            VkImageView stencil_image,
            std::optional<VkClearValue> const& clear_value = {});

    public:
        render_pass_t& operator=(render_pass_t const&) = default;

        render_pass_t& operator=(render_pass_t&&) noexcept = default;

    private:
        std::vector<VkRenderingAttachmentInfo> color_attachments_;
        std::optional<VkRenderingAttachmentInfo> depth_attachment_;
        std::optional<VkRenderingAttachmentInfo> stencil_attachment_;
    };
} // namespace vkrndr

#endif
