#ifndef VKRNDR_PIPELINE_INCLUDED
#define VKRNDR_PIPELINE_INCLUDED

#include <volk.h>

#include <cstdint>
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
    struct [[nodiscard]] pipeline_t final
    {
        std::shared_ptr<VkPipelineLayout> layout;
        VkPipeline pipeline{VK_NULL_HANDLE};
        VkPipelineBindPoint type{VK_PIPELINE_BIND_POINT_GRAPHICS};
    };

    void bind_pipeline(VkCommandBuffer command_buffer,
        pipeline_t const& pipeline,
        uint32_t first_set,
        std::span<VkDescriptorSet const> descriptor_sets);

    inline void bind_pipeline(VkCommandBuffer command_buffer,
        pipeline_t const& pipeline)
    {
        std::span<VkDescriptorSet const> const descriptor_sets;
        bind_pipeline(command_buffer, pipeline, 0, descriptor_sets);
    }

    void destroy(device_t* device, pipeline_t* pipeline);

    class [[nodiscard]] pipeline_layout_builder_t final
    {
    public:
        explicit pipeline_layout_builder_t(device_t& device);

        pipeline_layout_builder_t(pipeline_layout_builder_t const&) = default;

        pipeline_layout_builder_t(
            pipeline_layout_builder_t&&) noexcept = default;

    public:
        ~pipeline_layout_builder_t() = default;

    public:
        [[nodiscard]] std::shared_ptr<VkPipelineLayout> build();

        pipeline_layout_builder_t& add_descriptor_set_layout(
            VkDescriptorSetLayout descriptor_set_layout);

        pipeline_layout_builder_t& add_push_constants(
            VkPushConstantRange push_constant_range);

        template<typename T>
        pipeline_layout_builder_t&
        add_push_constants(VkShaderStageFlags stage_flags, uint32_t offset = 0);

    public:
        pipeline_layout_builder_t& operator=(
            pipeline_layout_builder_t const&) = default;

        pipeline_layout_builder_t& operator=(
            pipeline_layout_builder_t&&) = default;

    private:
        device_t* device_;
        std::vector<VkDescriptorSetLayout> descriptor_set_layouts_;
        std::vector<VkPushConstantRange> push_constants_;
    };

    template<typename T>
    pipeline_layout_builder_t& pipeline_layout_builder_t::add_push_constants(
        VkShaderStageFlags const stage_flags,
        uint32_t const offset)
    {
        // cppcheck-suppress-begin returnTempReference
        return add_push_constants(
            {.stageFlags = stage_flags, .offset = offset, .size = sizeof(T)});
        // cppcheck-suppress-end returnTempReference
    }

    class [[nodiscard]] pipeline_builder_t final
    {
    public: // Construction
        pipeline_builder_t(device_t& device,
            std::shared_ptr<VkPipelineLayout> pipeline_layout);

        pipeline_builder_t(pipeline_builder_t const&) = default;

        pipeline_builder_t(pipeline_builder_t&&) noexcept = default;

    public: // Destruction
        ~pipeline_builder_t();

    public: // Interface
        [[nodiscard]] pipeline_t build();

        pipeline_builder_t& add_shader(VkPipelineShaderStageCreateInfo shader);

        pipeline_builder_t& add_color_attachment(VkFormat format);

        pipeline_builder_t& add_vertex_input(
            std::span<VkVertexInputBindingDescription const>
                binding_descriptions,
            std::span<VkVertexInputAttributeDescription const>
                attribute_descriptions);

        pipeline_builder_t& with_rasterization_samples(
            VkSampleCountFlagBits samples);

        pipeline_builder_t& with_culling(VkCullModeFlags cull_mode,
            VkFrontFace front_face);

        pipeline_builder_t& with_primitive_topology(
            VkPrimitiveTopology primitive_topology);

        pipeline_builder_t& add_color_blending(
            VkPipelineColorBlendAttachmentState color_blending);

        pipeline_builder_t& with_depth_test(VkFormat depth_format,
            VkCompareOp compare = VK_COMPARE_OP_LESS);

        pipeline_builder_t& with_stencil_test(VkFormat depth_format,
            VkStencilOpState front,
            VkStencilOpState back);

        pipeline_builder_t& with_dynamic_state(VkDynamicState state);

    public: // Operators
        pipeline_builder_t& operator=(pipeline_builder_t const&) = default;

        pipeline_builder_t& operator=(pipeline_builder_t&&) noexcept = default;

    private: // Helpers
        void cleanup();

    private: // Data
        device_t* device_{};
        std::shared_ptr<VkPipelineLayout> pipeline_layout_;
        VkPrimitiveTopology primitive_topology_{
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
        std::vector<VkFormat> color_attachments_;
        std::vector<VkPipelineShaderStageCreateInfo> shaders_;
        std::vector<VkVertexInputBindingDescription> vertex_input_binding_;
        std::vector<VkVertexInputAttributeDescription> vertex_input_attributes_;
        VkSampleCountFlagBits rasterization_samples_{VK_SAMPLE_COUNT_1_BIT};
        VkCullModeFlags cull_mode_{VK_CULL_MODE_NONE};
        VkFrontFace front_face_{VK_FRONT_FACE_COUNTER_CLOCKWISE};
        std::vector<VkPipelineColorBlendAttachmentState> color_blending_;
        VkFormat depth_format_{VK_FORMAT_UNDEFINED};
        std::optional<VkPipelineDepthStencilStateCreateInfo> depth_stencil_;
        std::vector<VkDynamicState> dynamic_states_;
    };

    class [[nodiscard]] compute_pipeline_builder_t final
    {
    public:
        compute_pipeline_builder_t(device_t& device,
            std::shared_ptr<VkPipelineLayout> pipeline_layout);

        compute_pipeline_builder_t(compute_pipeline_builder_t const&) = default;

        compute_pipeline_builder_t(
            compute_pipeline_builder_t&&) noexcept = default;

    public: // Destruction
        ~compute_pipeline_builder_t() = default;

    public: // Interface
        [[nodiscard]] pipeline_t build();

        compute_pipeline_builder_t& with_shader(
            VkPipelineShaderStageCreateInfo shader);

    public: // Operators
        compute_pipeline_builder_t& operator=(
            compute_pipeline_builder_t const&) = default;

        compute_pipeline_builder_t& operator=(
            compute_pipeline_builder_t&&) noexcept = default;

    private: // Data
        device_t* device_{};
        std::shared_ptr<VkPipelineLayout> pipeline_layout_;
        VkPipelineShaderStageCreateInfo shader_{};
    };

} // namespace vkrndr

#endif
