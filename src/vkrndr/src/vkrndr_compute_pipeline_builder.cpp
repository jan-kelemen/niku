#include <vkrndr_compute_pipeline_builder.hpp>

#include <vkrndr_device.hpp>
#include <vkrndr_utility.hpp>

vkrndr::compute_pipeline_builder_t::compute_pipeline_builder_t(
    device_t const& device,
    std::shared_ptr<VkPipelineLayout> pipeline_layout)
    : device_{&device}
    , pipeline_layout_{std::move(pipeline_layout)}
{
}

vkrndr::pipeline_t vkrndr::compute_pipeline_builder_t::build()
{
    VkComputePipelineCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    create_info.layout = *pipeline_layout_;
    create_info.stage = shader_;

    VkPipeline pipeline; // NOLINT
    check_result(vkCreateComputePipelines(*device_,
        nullptr,
        1,
        &create_info,
        nullptr,
        &pipeline));

    pipeline_t rv{pipeline_layout_, pipeline, VK_PIPELINE_BIND_POINT_COMPUTE};

    return rv;
}

vkrndr::compute_pipeline_builder_t&
vkrndr::compute_pipeline_builder_t::with_shader(
    VkPipelineShaderStageCreateInfo const& shader)
{
    shader_ = shader;
    return *this;
}
