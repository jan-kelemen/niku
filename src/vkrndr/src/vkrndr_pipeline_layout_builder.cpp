#include <vkrndr_pipeline_layout_builder.hpp>

#include <vkrndr_device.hpp>
#include <vkrndr_pipeline.hpp>
#include <vkrndr_utility.hpp>

#include <vulkan/utility/vk_struct_helper.hpp>

vkrndr::pipeline_layout_builder_t::pipeline_layout_builder_t(
    device_t const& device)
    : device_{&device}
{
}

vkrndr::pipeline_layout_t vkrndr::pipeline_layout_builder_t::build()
{
    VkPipelineLayoutCreateInfo const pipeline_layout_info{
        .sType = vku::GetSType<VkPipelineLayoutCreateInfo>(),
        .setLayoutCount = count_cast(descriptor_set_layouts_.size()),
        .pSetLayouts = descriptor_set_layouts_.data(),
        .pushConstantRangeCount = count_cast(push_constants_.size()),
        .pPushConstantRanges = push_constants_.data(),
    };

    pipeline_layout_t rv;
    check_result(vkCreatePipelineLayout(*device_,
        &pipeline_layout_info,
        nullptr,
        &rv.handle));

    return rv;
}

vkrndr::pipeline_layout_builder_t&
vkrndr::pipeline_layout_builder_t::add_descriptor_set_layout(
    VkDescriptorSetLayout const descriptor_set_layout)
{
    descriptor_set_layouts_.push_back(descriptor_set_layout);
    return *this;
}

vkrndr::pipeline_layout_builder_t&
vkrndr::pipeline_layout_builder_t::add_push_constants(
    VkPushConstantRange const push_constant_range)
{
    push_constants_.push_back(push_constant_range);
    return *this;
}
