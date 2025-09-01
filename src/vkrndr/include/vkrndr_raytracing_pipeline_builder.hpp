#ifndef VKRNDR_RAYTRACING_PIPELINE_BUILDER_INCLUDED
#define VKRNDR_RAYTRACING_PIPELINE_BUILDER_INCLUDED

#include <vkrndr_pipeline.hpp>

#include <volk.h>

#include <memory>

namespace vkrndr
{
    struct device_t;
} // namespace vkrndr

namespace vkrndr
{
    class [[nodiscard]] raytracing_pipeline_builder_t final
    {
    public:
        raytracing_pipeline_builder_t(device_t const& device,
            pipeline_layout_t const& layout);

        raytracing_pipeline_builder_t(
            raytracing_pipeline_builder_t const&) = default;

        raytracing_pipeline_builder_t(
            raytracing_pipeline_builder_t&&) noexcept = default;

    public:
        ~raytracing_pipeline_builder_t() = default;

    public:
        pipeline_t build();

        raytracing_pipeline_builder_t& add_shader(
            VkPipelineShaderStageCreateInfo const& shader,
            uint32_t& stage);

        raytracing_pipeline_builder_t& add_group(
            VkRayTracingShaderGroupCreateInfoKHR const& group);

        raytracing_pipeline_builder_t& with_recursion_depth(uint32_t depth);

    public:
        raytracing_pipeline_builder_t& operator=(
            raytracing_pipeline_builder_t const&) = default;

        raytracing_pipeline_builder_t& operator=(
            raytracing_pipeline_builder_t&&) noexcept = default;

    private:
        device_t const* device_{};
        pipeline_layout_t const* layout_{};
        uint32_t recursion_depth_{1};
        std::vector<VkPipelineShaderStageCreateInfo> stages_;
        std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups_;
    };

    [[nodiscard]] VkRayTracingShaderGroupCreateInfoKHR
    general_shader(VkRayTracingShaderGroupTypeKHR type, uint32_t index);

    [[nodiscard]] VkRayTracingShaderGroupCreateInfoKHR
    closest_hit_shader(VkRayTracingShaderGroupTypeKHR type, uint32_t index);

    [[nodiscard]] VkRayTracingShaderGroupCreateInfoKHR
    any_hit_shader(VkRayTracingShaderGroupTypeKHR type, uint32_t index);

    [[nodiscard]] VkRayTracingShaderGroupCreateInfoKHR
    intersection_shader(VkRayTracingShaderGroupTypeKHR type, uint32_t index);
} // namespace vkrndr

#endif
