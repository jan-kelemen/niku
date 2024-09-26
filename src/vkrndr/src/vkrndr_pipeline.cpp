#include <vkrndr_pipeline.hpp>

#include <vkrndr_device.hpp>
#include <vkrndr_utility.hpp>

#include <cppext_pragma_warning.hpp>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iterator>
#include <memory>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

// IWYU pragma: no_include <type_traits>
// IWYU pragma: no_include <functional>

void vkrndr::bind_pipeline(VkCommandBuffer command_buffer,
    pipeline_t const& pipeline,
    uint32_t const first_set,
    std::span<VkDescriptorSet const> descriptor_sets)
{
    if (!descriptor_sets.empty())
    {
        vkCmdBindDescriptorSets(command_buffer,
            pipeline.type,
            *pipeline.layout,
            first_set,
            count_cast(descriptor_sets.size()),
            descriptor_sets.data(),
            0,
            nullptr);
    }

    vkCmdBindPipeline(command_buffer, pipeline.type, pipeline.pipeline);
}

void vkrndr::destroy(device_t* const device, pipeline_t* const pipeline)
{
    if (pipeline)
    {
        vkDestroyPipeline(device->logical, pipeline->pipeline, nullptr);
        if (pipeline->layout.use_count() == 1)
        {
            vkDestroyPipelineLayout(device->logical,
                *pipeline->layout,
                nullptr);
        }
        pipeline->layout.reset();
    }
}

vkrndr::pipeline_layout_builder_t::pipeline_layout_builder_t(device_t& device)
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
    vkrndr::check_result(vkCreatePipelineLayout(device_->logical,
        &pipeline_layout_info,
        nullptr,
        &rv));

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

vkrndr::pipeline_builder_t::pipeline_builder_t(device_t& device,
    std::shared_ptr<VkPipelineLayout> pipeline_layout,
    VkFormat const image_format)
    : device_{&device}
    , pipeline_layout_{std::move(pipeline_layout)}
    , image_format_{image_format}
    , dynamic_states_{{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR}}
{
}

vkrndr::pipeline_builder_t::~pipeline_builder_t() { cleanup(); }

vkrndr::pipeline_t vkrndr::pipeline_builder_t::build()
{
    VkPipelineVertexInputStateCreateInfo vertex_input_info{};
    vertex_input_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount =
        count_cast(vertex_input_binding_.size());
    vertex_input_info.pVertexBindingDescriptions = vertex_input_binding_.data();
    vertex_input_info.vertexAttributeDescriptionCount =
        count_cast(vertex_input_attributes_.size());
    vertex_input_info.pVertexAttributeDescriptions =
        vertex_input_attributes_.data();

    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = primitive_topology_;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = cull_mode_;
    rasterizer.frontFace = front_face_;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_TRUE;
    multisampling.minSampleShading = .2f;
    multisampling.rasterizationSamples = rasterization_samples_;

    DISABLE_WARNING_PUSH
    DISABLE_WARNING_MISSING_FIELD_INITIALIZERS
    VkPipelineColorBlendAttachmentState const color_blend_attachment{
        color_blending_.value_or(
            VkPipelineColorBlendAttachmentState{.blendEnable = VK_FALSE,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                    VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                    VK_COLOR_COMPONENT_A_BIT})};
    DISABLE_WARNING_POP

    VkPipelineColorBlendStateCreateInfo color_blending{};
    color_blending.sType =
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.logicOp = VK_LOGIC_OP_COPY;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;
    std::ranges::fill(color_blending.blendConstants, 0.0f);

    VkPipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    dynamic_state.dynamicStateCount = count_cast(dynamic_states_.size()),
    dynamic_state.pDynamicStates = dynamic_states_.data();

    VkPipelineRenderingCreateInfo rendering_create_info{};
    rendering_create_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    rendering_create_info.colorAttachmentCount = 1;
    rendering_create_info.pColorAttachmentFormats = &image_format_;
    if (depth_stencil_)
    {
        rendering_create_info.depthAttachmentFormat = depth_format_;
        if (depth_stencil_->stencilTestEnable)
        {
            rendering_create_info.stencilAttachmentFormat = depth_format_;
        }
    }

    VkGraphicsPipelineCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    create_info.pVertexInputState = &vertex_input_info;
    create_info.pInputAssemblyState = &input_assembly;
    create_info.pRasterizationState = &rasterizer;
    create_info.pColorBlendState = &color_blending;
    create_info.pMultisampleState = &multisampling;
    create_info.pViewportState = &viewport_state;
    create_info.pDynamicState = &dynamic_state;
    if (depth_stencil_)
    {
        create_info.pDepthStencilState = &depth_stencil_.value();
    }
    create_info.stageCount = count_cast(shaders_.size());
    create_info.pStages = shaders_.data();
    create_info.layout = *pipeline_layout_;
    create_info.pNext = &rendering_create_info;

    VkPipeline pipeline; // NOLINT
    check_result(vkCreateGraphicsPipelines(device_->logical,
        VK_NULL_HANDLE,
        1,
        &create_info,
        nullptr,
        &pipeline));

    pipeline_t rv{pipeline_layout_, pipeline, VK_PIPELINE_BIND_POINT_GRAPHICS};

    cleanup();

    return rv;
}

vkrndr::pipeline_builder_t& vkrndr::pipeline_builder_t::add_shader(
    VkPipelineShaderStageCreateInfo const shader)
{
    shaders_.push_back(shader);

    return *this;
}

vkrndr::pipeline_builder_t& vkrndr::pipeline_builder_t::add_vertex_input(
    std::span<VkVertexInputBindingDescription const> binding_descriptions,
    std::span<VkVertexInputAttributeDescription const> attribute_descriptions)
{
    vertex_input_binding_.reserve(
        vertex_input_binding_.size() + binding_descriptions.size());
    vertex_input_attributes_.reserve(
        vertex_input_attributes_.size() + attribute_descriptions.size());

    vertex_input_binding_.insert(vertex_input_binding_.cend(),
        binding_descriptions.begin(),
        binding_descriptions.end());
    vertex_input_attributes_.insert(vertex_input_attributes_.cend(),
        attribute_descriptions.begin(),
        attribute_descriptions.end());
    return *this;
}

vkrndr::pipeline_builder_t&
vkrndr::pipeline_builder_t::with_rasterization_samples(
    VkSampleCountFlagBits const samples)
{
    rasterization_samples_ = samples;
    return *this;
}

vkrndr::pipeline_builder_t& vkrndr::pipeline_builder_t::with_culling(
    VkCullModeFlags const cull_mode,
    VkFrontFace const front_face)
{
    cull_mode_ = cull_mode;
    front_face_ = front_face;

    return *this;
}

vkrndr::pipeline_builder_t& vkrndr::pipeline_builder_t::with_primitive_topology(
    VkPrimitiveTopology const primitive_topology)
{
    primitive_topology_ = primitive_topology;

    return *this;
}

vkrndr::pipeline_builder_t& vkrndr::pipeline_builder_t::with_color_blending(
    VkPipelineColorBlendAttachmentState const color_blending)
{
    color_blending_ = color_blending;

    return *this;
}

vkrndr::pipeline_builder_t& vkrndr::pipeline_builder_t::with_depth_test(
    VkFormat const depth_format,
    VkCompareOp const compare)
{
    assert(
        depth_format_ == VK_FORMAT_UNDEFINED || depth_format_ == depth_format);

    depth_format_ = depth_format;

    DISABLE_WARNING_PUSH
    DISABLE_WARNING_MISSING_FIELD_INITIALIZERS
    VkPipelineDepthStencilStateCreateInfo depth_stencil{
        depth_stencil_.value_or(VkPipelineDepthStencilStateCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable = VK_FALSE,
            .front = {},
            .back = {},
            .minDepthBounds = 0.0f,
            .maxDepthBounds = 1.0f,
        })};
    DISABLE_WARNING_POP

    auto const enabled{
        depth_format_ != VK_FORMAT_UNDEFINED ? VK_TRUE : VK_FALSE};

    depth_stencil.depthTestEnable = enabled;
    depth_stencil.depthWriteEnable = enabled;
    depth_stencil.depthCompareOp = compare;

    depth_stencil_ = depth_stencil;

    return *this;
}

vkrndr::pipeline_builder_t& vkrndr::pipeline_builder_t::with_stencil_test(
    VkFormat depth_format,
    VkStencilOpState front,
    VkStencilOpState back)
{
    assert(
        depth_format_ == VK_FORMAT_UNDEFINED || depth_format_ == depth_format);

    depth_format_ = depth_format;

    DISABLE_WARNING_PUSH
    DISABLE_WARNING_MISSING_FIELD_INITIALIZERS
    VkPipelineDepthStencilStateCreateInfo depth_stencil{
        depth_stencil_.value_or(VkPipelineDepthStencilStateCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = VK_FALSE,
            .depthWriteEnable = VK_FALSE,
            .depthCompareOp = VK_COMPARE_OP_LESS,
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable = VK_FALSE,
            .front = {},
            .back = {},
            .minDepthBounds = 0.0f,
            .maxDepthBounds = 1.0f,
        })};
    DISABLE_WARNING_POP

    auto const enabled{
        depth_format_ != VK_FORMAT_UNDEFINED ? VK_TRUE : VK_FALSE};
    depth_stencil.stencilTestEnable = enabled;
    depth_stencil.front = front;
    depth_stencil.back = back;

    depth_stencil_ = depth_stencil;

    return *this;
}

vkrndr::pipeline_builder_t& vkrndr::pipeline_builder_t::with_dynamic_state(
    VkDynamicState state)
{
    if (std::ranges::find(dynamic_states_, state) == std::cend(dynamic_states_))
    {
        dynamic_states_.push_back(state);
    }
    return *this;
}

void vkrndr::pipeline_builder_t::cleanup()
{
    vertex_input_attributes_.clear();
    vertex_input_binding_.clear();
    shaders_.clear();
    pipeline_layout_.reset();
}

vkrndr::compute_pipeline_builder_t::compute_pipeline_builder_t(device_t& device,
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
    check_result(vkCreateComputePipelines(device_->logical,
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
    VkPipelineShaderStageCreateInfo shader)
{
    shader_ = shader;
    return *this;
}
