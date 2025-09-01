#include <vkrndr_compute_pipeline_builder.hpp>

#include <vkrndr_device.hpp>
#include <vkrndr_utility.hpp>

#include <vulkan/utility/vk_struct_helper.hpp>

vkrndr::compute_pipeline_builder_t::compute_pipeline_builder_t(
    device_t const& device,
    pipeline_layout_t const& layout)
    : device_{&device}
    , layout_{&layout}
{
}

vkrndr::pipeline_t vkrndr::compute_pipeline_builder_t::build()
{
    VkComputePipelineCreateInfo const create_info{
        .sType = vku::GetSType<VkComputePipelineCreateInfo>(),
        .stage = shader_,
        .layout = *layout_,
    };

    pipeline_t rv{*layout_, VK_NULL_HANDLE, VK_PIPELINE_BIND_POINT_COMPUTE};
    check_result(vkCreateComputePipelines(*device_,
        nullptr,
        1,
        &create_info,
        nullptr,
        &rv.handle));

    return rv;
}

vkrndr::compute_pipeline_builder_t&
vkrndr::compute_pipeline_builder_t::with_shader(
    VkPipelineShaderStageCreateInfo const& shader)
{
    shader_ = shader;
    return *this;
}
