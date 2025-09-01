#ifndef VKRNDR_GRAPHICS_PIPELINE_BUILDER_INCLUDED
#define VKRNDR_GRAPHICS_PIPELINE_BUILDER_INCLUDED

#include <vkrndr_pipeline.hpp>

#include <volk.h>

#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace vkrndr
{
    struct device_t;
} // namespace vkrndr

namespace vkrndr
{
    class [[nodiscard]] graphics_pipeline_builder_t final
    {
    public:
        graphics_pipeline_builder_t(device_t const& device,
            pipeline_layout_t const& layout);

        graphics_pipeline_builder_t(
            graphics_pipeline_builder_t const&) = default;

        graphics_pipeline_builder_t(
            graphics_pipeline_builder_t&&) noexcept = default;

    public:
        ~graphics_pipeline_builder_t() = default;

    public:
        pipeline_t build();

        graphics_pipeline_builder_t& add_shader(
            VkPipelineShaderStageCreateInfo shader);

        graphics_pipeline_builder_t& add_color_attachment(VkFormat format,
            std::optional<VkPipelineColorBlendAttachmentState> const&
                color_blending = {});

        graphics_pipeline_builder_t& add_vertex_input(
            std::span<VkVertexInputBindingDescription const> const&
                binding_descriptions,
            std::span<VkVertexInputAttributeDescription const> const&
                attribute_descriptions);

        graphics_pipeline_builder_t& with_rasterization_samples(
            VkSampleCountFlagBits samples);

        graphics_pipeline_builder_t& with_culling(VkCullModeFlags cull_mode,
            VkFrontFace front_face);

        graphics_pipeline_builder_t& with_primitive_topology(
            VkPrimitiveTopology primitive_topology);

        graphics_pipeline_builder_t& with_depth_test(VkFormat depth_format,
            VkCompareOp compare = VK_COMPARE_OP_LESS,
            bool write = true);

        graphics_pipeline_builder_t& with_stencil_test(VkFormat depth_format,
            VkStencilOpState front,
            VkStencilOpState back);

        graphics_pipeline_builder_t& with_tesselation_patch_points(
            uint32_t points);

        graphics_pipeline_builder_t& with_dynamic_state(VkDynamicState state);

    public:
        graphics_pipeline_builder_t& operator=(
            graphics_pipeline_builder_t const&) = default;

        graphics_pipeline_builder_t& operator=(
            graphics_pipeline_builder_t&&) noexcept = default;

    private:
        device_t const* device_{};
        pipeline_layout_t const* layout_;
        VkPrimitiveTopology primitive_topology_{
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
        std::vector<VkFormat> color_attachments_;
        std::vector<VkPipelineColorBlendAttachmentState> color_blending_;
        std::vector<VkPipelineShaderStageCreateInfo> shaders_;
        std::vector<VkVertexInputBindingDescription> vertex_input_binding_;
        std::vector<VkVertexInputAttributeDescription> vertex_input_attributes_;
        VkSampleCountFlagBits rasterization_samples_{VK_SAMPLE_COUNT_1_BIT};
        VkCullModeFlags cull_mode_{VK_CULL_MODE_NONE};
        VkFrontFace front_face_{VK_FRONT_FACE_COUNTER_CLOCKWISE};
        VkFormat depth_format_{VK_FORMAT_UNDEFINED};
        std::optional<VkPipelineDepthStencilStateCreateInfo> depth_stencil_;
        std::optional<VkPipelineTessellationStateCreateInfo> tesselation_;
        std::vector<VkDynamicState> dynamic_states_;
    };
} // namespace vkrndr
#endif
