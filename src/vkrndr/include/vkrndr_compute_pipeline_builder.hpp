#ifndef VKRNDR_COMPUTE_PIPELINE_BUILDER_INCLUDED
#define VKRNDR_COMPUTE_PIPELINE_BUILDER_INCLUDED

#include <vkrndr_pipeline.hpp>

#include <volk.h>

#include <memory>

namespace vkrndr
{
    struct device_t;
} // namespace vkrndr

namespace vkrndr
{
    class [[nodiscard]] compute_pipeline_builder_t final
    {
    public:
        compute_pipeline_builder_t(device_t const& device,
            std::shared_ptr<VkPipelineLayout> pipeline_layout);

        compute_pipeline_builder_t(compute_pipeline_builder_t const&) = default;

        compute_pipeline_builder_t(
            compute_pipeline_builder_t&&) noexcept = default;

    public: // Destruction
        ~compute_pipeline_builder_t() = default;

    public: // Interface
        [[nodiscard]] pipeline_t build();

        compute_pipeline_builder_t& with_shader(
            VkPipelineShaderStageCreateInfo const& shader);

    public: // Operators
        compute_pipeline_builder_t& operator=(
            compute_pipeline_builder_t const&) = default;

        compute_pipeline_builder_t& operator=(
            compute_pipeline_builder_t&&) noexcept = default;

    private: // Data
        device_t const* device_{};
        std::shared_ptr<VkPipelineLayout> pipeline_layout_;
        VkPipelineShaderStageCreateInfo shader_{};
    };
} // namespace vkrndr
#endif
