#ifndef VKRNDR_VULKAN_PIPELINE_INCLUDED
#define VKRNDR_VULKAN_PIPELINE_INCLUDED

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace vkrndr
{
    struct vulkan_device;
} // namespace vkrndr

namespace vkrndr
{
    struct [[nodiscard]] vulkan_pipeline final
    {
        std::shared_ptr<VkPipelineLayout> layout;
        VkPipeline pipeline;
        VkPipelineBindPoint type;
    };

    void bind_pipeline(VkCommandBuffer command_buffer,
        vulkan_pipeline const& pipeline,
        uint32_t first_set,
        std::span<VkDescriptorSet const> descriptor_sets);

    inline void bind_pipeline(VkCommandBuffer command_buffer,
        vulkan_pipeline const& pipeline)
    {
        std::span<VkDescriptorSet const> const descriptor_sets;
        bind_pipeline(command_buffer, pipeline, 0, descriptor_sets);
    }

    void destroy(vulkan_device* device, vulkan_pipeline* pipeline);

    class [[nodiscard]] vulkan_pipeline_layout_builder final
    {
    public:
        explicit vulkan_pipeline_layout_builder(vulkan_device* device);

        vulkan_pipeline_layout_builder(
            vulkan_pipeline_layout_builder const&) = delete;

        vulkan_pipeline_layout_builder(
            vulkan_pipeline_layout_builder&&) noexcept = delete;

    public:
        ~vulkan_pipeline_layout_builder() = default;

    public:
        [[nodiscard]] std::shared_ptr<VkPipelineLayout> build();

        vulkan_pipeline_layout_builder& add_descriptor_set_layout(
            VkDescriptorSetLayout descriptor_set_layout);

        vulkan_pipeline_layout_builder& add_push_constants(
            VkPushConstantRange push_constant_range);

    public:
        vulkan_pipeline_layout_builder& operator=(
            vulkan_pipeline_layout_builder const&) = delete;

        vulkan_pipeline_layout_builder& operator=(
            vulkan_pipeline_layout_builder&&) = delete;

    private:
        vulkan_device* device_;
        std::vector<VkDescriptorSetLayout> descriptor_set_layouts_;
        std::vector<VkPushConstantRange> push_constants_;
    };

    class [[nodiscard]] vulkan_pipeline_builder final
    {
    public: // Construction
        vulkan_pipeline_builder(vulkan_device* device,
            std::shared_ptr<VkPipelineLayout> pipeline_layout,
            VkFormat image_format);

        vulkan_pipeline_builder(vulkan_pipeline_builder const&) = delete;

        vulkan_pipeline_builder(vulkan_pipeline_builder&&) noexcept = delete;

    public: // Destruction
        ~vulkan_pipeline_builder();

    public: // Interface
        [[nodiscard]] vulkan_pipeline build();

        vulkan_pipeline_builder& add_shader(VkShaderStageFlagBits stage,
            std::filesystem::path const& path,
            std::string_view entry_point);

        vulkan_pipeline_builder& add_vertex_input(
            std::span<VkVertexInputBindingDescription const>
                binding_descriptions,
            std::span<VkVertexInputAttributeDescription const>
                attribute_descriptions);

        vulkan_pipeline_builder& with_rasterization_samples(
            VkSampleCountFlagBits samples);

        vulkan_pipeline_builder& with_culling(VkCullModeFlags cull_mode,
            VkFrontFace front_face);

        vulkan_pipeline_builder& with_primitive_topology(
            VkPrimitiveTopology primitive_topology);

        vulkan_pipeline_builder& with_color_blending(
            VkPipelineColorBlendAttachmentState color_blending);

        vulkan_pipeline_builder& with_depth_test(VkFormat depth_format,
            VkCompareOp compare = VK_COMPARE_OP_LESS);

        vulkan_pipeline_builder& with_stencil_test(VkFormat depth_format,
            VkStencilOpState front,
            VkStencilOpState back);

        vulkan_pipeline_builder& with_dynamic_state(VkDynamicState state);

    public: // Operators
        vulkan_pipeline_builder& operator=(
            vulkan_pipeline_builder const&) = delete;

        vulkan_pipeline_builder& operator=(
            vulkan_pipeline_builder&&) noexcept = delete;

    private: // Helpers
        void cleanup();

    private: // Data
        vulkan_device* device_{};
        std::shared_ptr<VkPipelineLayout> pipeline_layout_;
        VkPrimitiveTopology primitive_topology_{
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
        VkFormat image_format_{};
        std::vector<
            std::tuple<VkShaderStageFlagBits, VkShaderModule, std::string>>
            shaders_;
        std::vector<VkVertexInputBindingDescription> vertex_input_binding_;
        std::vector<VkVertexInputAttributeDescription> vertex_input_attributes_;
        VkSampleCountFlagBits rasterization_samples_{VK_SAMPLE_COUNT_1_BIT};
        VkCullModeFlags cull_mode_{VK_CULL_MODE_NONE};
        VkFrontFace front_face_{VK_FRONT_FACE_COUNTER_CLOCKWISE};
        std::optional<VkPipelineColorBlendAttachmentState> color_blending_;
        VkFormat depth_format_{VK_FORMAT_UNDEFINED};
        std::optional<VkPipelineDepthStencilStateCreateInfo> depth_stencil_;
        std::vector<VkDynamicState> dynamic_states_;
    };

    class [[nodiscard]] vulkan_compute_pipeline_builder final
    {
    public:
        vulkan_compute_pipeline_builder(vulkan_device* device,
            std::shared_ptr<VkPipelineLayout> pipeline_layout);

        vulkan_compute_pipeline_builder(
            vulkan_compute_pipeline_builder const&) = delete;

        vulkan_compute_pipeline_builder(
            vulkan_compute_pipeline_builder&&) noexcept = delete;

    public: // Destruction
        ~vulkan_compute_pipeline_builder();

    public: // Interface
        [[nodiscard]] vulkan_pipeline build();

        vulkan_compute_pipeline_builder& with_shader(
            std::filesystem::path const& path,
            std::string_view entry_point);

    public: // Operators
        vulkan_compute_pipeline_builder& operator=(
            vulkan_compute_pipeline_builder const&) = delete;

        vulkan_compute_pipeline_builder& operator=(
            vulkan_compute_pipeline_builder&&) noexcept = delete;

    private: // Helpers
        void cleanup();

    private: // Data
        vulkan_device* device_{};
        std::shared_ptr<VkPipelineLayout> pipeline_layout_;
        VkShaderModule shader_module_{VK_NULL_HANDLE};
        std::string shader_entry_point_;
    };
} // namespace vkrndr

#endif // !VKRNDR_VULKAN_PIPELINE_INCLUDED
