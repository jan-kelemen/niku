#include <vkrndr_raytracing_pipeline_builder.hpp>

#include <vkrndr_device.hpp>
#include <vkrndr_pipeline.hpp>
#include <vkrndr_utility.hpp>

#include <vulkan/utility/vk_struct_helper.hpp>

vkrndr::raytracing_pipeline_builder_t::raytracing_pipeline_builder_t(
    device_t const& device,
    pipeline_layout_t const& layout)
    : device_{&device}
    , layout_{&layout}
{
}

vkrndr::pipeline_t vkrndr::raytracing_pipeline_builder_t::build()
{
    VkRayTracingPipelineCreateInfoKHR const create_info{
        .sType = vku::GetSType<VkRayTracingPipelineCreateInfoKHR>(),
        .stageCount = count_cast(stages_),
        .pStages = stages_.data(),
        .groupCount = count_cast(groups_),
        .pGroups = groups_.data(),
        .maxPipelineRayRecursionDepth = recursion_depth_,
        .layout = *layout_,
    };

    pipeline_t rv{*layout_,
        VK_NULL_HANDLE,
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR};
    check_result(vkCreateRayTracingPipelinesKHR(*device_,
        VK_NULL_HANDLE,
        VK_NULL_HANDLE,
        1,
        &create_info,
        nullptr,
        &rv.handle));

    return rv;
}

vkrndr::raytracing_pipeline_builder_t&
vkrndr::raytracing_pipeline_builder_t::add_shader(
    VkPipelineShaderStageCreateInfo const& shader,
    uint32_t& stage)
{
    stages_.push_back(shader);

    stage = count_cast(stages_) - 1;

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
