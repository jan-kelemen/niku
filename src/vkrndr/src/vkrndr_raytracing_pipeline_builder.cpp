#include <vkrndr_raytracing_pipeline_builder.hpp>

#include <vkrndr_device.hpp>
#include <vkrndr_utility.hpp>

#include <vulkan/utility/vk_struct_helper.hpp>

vkrndr::raytracing_pipeline_builder_t::raytracing_pipeline_builder_t(
    device_t const& device,
    std::shared_ptr<VkPipelineLayout> pipeline_layout)
    : device_{&device}
    , pipeline_layout_{std::move(pipeline_layout)}
{
}

vkrndr::raytracing_pipeline_builder_t::~raytracing_pipeline_builder_t()
{
    cleanup();
}

vkrndr::pipeline_t vkrndr::raytracing_pipeline_builder_t::build()
{
    VkRayTracingPipelineCreateInfoKHR const create_info{
        .sType = vku::GetSType<VkRayTracingPipelineCreateInfoKHR>(),
        .stageCount = count_cast(stages_.size()),
        .pStages = stages_.data(),
        .groupCount = count_cast(groups_.size()),
        .pGroups = groups_.data(),
        .maxPipelineRayRecursionDepth = recursion_depth_,
        .layout = *pipeline_layout_,
    };

    VkPipeline pipeline; // NOLINT
    check_result(vkCreateRayTracingPipelinesKHR(*device_,
        VK_NULL_HANDLE,
        VK_NULL_HANDLE,
        1,
        &create_info,
        nullptr,
        &pipeline));

    pipeline_t rv{pipeline_layout_,
        pipeline,
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR};

    cleanup();

    return rv;
}

vkrndr::raytracing_pipeline_builder_t&
vkrndr::raytracing_pipeline_builder_t::add_shader(
    VkPipelineShaderStageCreateInfo const& shader,
    uint32_t& stage)
{
    stages_.push_back(shader);

    stage = count_cast(stages_.size()) - 1;

    return *this;
}

vkrndr::raytracing_pipeline_builder_t&
vkrndr::raytracing_pipeline_builder_t::add_group(
    VkRayTracingShaderGroupCreateInfoKHR const& group)
{
    groups_.push_back(group);

    return *this;
}

vkrndr::raytracing_pipeline_builder_t&
vkrndr::raytracing_pipeline_builder_t::with_recursion_depth(
    uint32_t const depth)
{
    recursion_depth_ = depth;

    return *this;
}

void vkrndr::raytracing_pipeline_builder_t::cleanup()
{
    groups_.clear();
    stages_.clear();
    pipeline_layout_.reset();
}

VkRayTracingShaderGroupCreateInfoKHR vkrndr::general_shader(
    VkRayTracingShaderGroupTypeKHR const type,
    uint32_t const index)
{
    return {
        .sType = vku::GetSType<VkRayTracingShaderGroupCreateInfoKHR>(),
        .type = type,
        .generalShader = index,
        .closestHitShader = VK_SHADER_UNUSED_KHR,
        .anyHitShader = VK_SHADER_UNUSED_KHR,
        .intersectionShader = VK_SHADER_UNUSED_KHR,
    };
}

VkRayTracingShaderGroupCreateInfoKHR vkrndr::closest_hit_shader(
    VkRayTracingShaderGroupTypeKHR const type,
    uint32_t const index)
{
    return {
        .sType = vku::GetSType<VkRayTracingShaderGroupCreateInfoKHR>(),
        .type = type,
        .generalShader = VK_SHADER_UNUSED_KHR,
        .closestHitShader = index,
        .anyHitShader = VK_SHADER_UNUSED_KHR,
        .intersectionShader = VK_SHADER_UNUSED_KHR,
    };
}

VkRayTracingShaderGroupCreateInfoKHR vkrndr::any_hit_shader(
    VkRayTracingShaderGroupTypeKHR const type,
    uint32_t const index)
{
    return {
        .sType = vku::GetSType<VkRayTracingShaderGroupCreateInfoKHR>(),
        .type = type,
        .generalShader = VK_SHADER_UNUSED_KHR,
        .closestHitShader = VK_SHADER_UNUSED_KHR,
        .anyHitShader = index,
        .intersectionShader = VK_SHADER_UNUSED_KHR,
    };
}

VkRayTracingShaderGroupCreateInfoKHR vkrndr::intersection_shader(
    VkRayTracingShaderGroupTypeKHR const type,
    uint32_t const index)
{
    return {
        .sType = vku::GetSType<VkRayTracingShaderGroupCreateInfoKHR>(),
        .type = type,
        .generalShader = VK_SHADER_UNUSED_KHR,
        .closestHitShader = VK_SHADER_UNUSED_KHR,
        .anyHitShader = VK_SHADER_UNUSED_KHR,
        .intersectionShader = index,
    };
}
