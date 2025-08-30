#include <vkrndr_pipeline_layout_builder.hpp>

#include <vkrndr_device.hpp>
#include <vkrndr_utility.hpp>

vkrndr::pipeline_layout_builder_t::pipeline_layout_builder_t(
    device_t const& device)
    : device_{&device}
{
}

std::shared_ptr<VkPipelineLayout> vkrndr::pipeline_layout_builder_t::build()
{
    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    pipeline_layout_info.pushConstantRangeCount =
        count_cast(push_constants_.size());
    pipeline_layout_info.pPushConstantRanges = push_constants_.data();

    pipeline_layout_info.setLayoutCount =
        count_cast(descriptor_set_layouts_.size());
    pipeline_layout_info.pSetLayouts = descriptor_set_layouts_.data();

    VkPipelineLayout rv; // NOLINT
    check_result(
        vkCreatePipelineLayout(*device_, &pipeline_layout_info, nullptr, &rv));

    return std::make_shared<VkPipelineLayout>(rv);
}

vkrndr::pipeline_layout_builder_t&
vkrndr::pipeline_layout_builder_t::add_descriptor_set_layout(
    VkDescriptorSetLayout descriptor_set_layout)
{
    descriptor_set_layouts_.push_back(descriptor_set_layout);
    return *this;
}

vkrndr::pipeline_layout_builder_t&
vkrndr::pipeline_layout_builder_t::add_push_constants(
    VkPushConstantRange push_constant_range)
{
    push_constants_.push_back(push_constant_range);
    return *this;
}
