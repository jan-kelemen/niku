#include <vkrndr_graphics_pipeline_builder.hpp>

#include <vkrndr_device.hpp>
#include <vkrndr_pipeline.hpp>
#include <vkrndr_utility.hpp>

#include <vulkan/utility/vk_struct_helper.hpp>

#include <algorithm>
#include <cassert>
#include <utility> // IWYU pragma: keep

// IWYU pragma: no_include <iterator>
// IWYU pragma: no_include <functional>

vkrndr::graphics_pipeline_builder_t::graphics_pipeline_builder_t(
    device_t const& device,
    pipeline_layout_t const& layout)
    : device_{&device}
    , layout_{&layout}
    , dynamic_states_{{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR}}
{
}

vkrndr::pipeline_t vkrndr::graphics_pipeline_builder_t::build()
{
    VkPipelineVertexInputStateCreateInfo const vertex_input_info{
        .sType = vku::GetSType<VkPipelineVertexInputStateCreateInfo>(),
        .vertexBindingDescriptionCount =
            count_cast(vertex_input_binding_.size()),
        .pVertexBindingDescriptions = vertex_input_binding_.data(),
        .vertexAttributeDescriptionCount =
            count_cast(vertex_input_attributes_.size()),
        .pVertexAttributeDescriptions = vertex_input_attributes_.data(),
    };

    VkPipelineInputAssemblyStateCreateInfo const input_assembly{
        .sType = vku::GetSType<VkPipelineInputAssemblyStateCreateInfo>(),
        .topology = primitive_topology_,
    };

    VkPipelineViewportStateCreateInfo const viewport_state{
        .sType = vku::GetSType<VkPipelineViewportStateCreateInfo>(),
        .viewportCount = 1,
        .scissorCount = 1,
    };

    VkPipelineRasterizationStateCreateInfo const rasterizer{
        .sType = vku::GetSType<VkPipelineRasterizationStateCreateInfo>(),
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = cull_mode_,
        .frontFace = front_face_,
        .lineWidth = 1.0f,
    };

    VkPipelineMultisampleStateCreateInfo const multisampling{
        .sType = vku::GetSType<VkPipelineMultisampleStateCreateInfo>(),
        .rasterizationSamples = rasterization_samples_,
        .sampleShadingEnable = VK_TRUE,
        .minSampleShading = .2f,
    };

    VkPipelineColorBlendStateCreateInfo const color_blending{
        .sType = vku::GetSType<VkPipelineColorBlendStateCreateInfo>(),
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = count_cast(color_blending_.size()),
        .pAttachments = color_blending_.data(),
        .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f},
    };

    VkPipelineDynamicStateCreateInfo const dynamic_state{
        .sType = vku::GetSType<VkPipelineDynamicStateCreateInfo>(),
        .dynamicStateCount = count_cast(dynamic_states_.size()),
        .pDynamicStates = dynamic_states_.data(),
    };

    VkPipelineRenderingCreateInfo rendering_create_info{
        .sType = vku::GetSType<VkPipelineRenderingCreateInfo>(),
        .colorAttachmentCount = count_cast(color_attachments_.size()),
        .pColorAttachmentFormats = color_attachments_.data(),
    };
    if (depth_stencil_)
    {
        rendering_create_info.depthAttachmentFormat = depth_format_;
        if (depth_stencil_->stencilTestEnable)
        {
            rendering_create_info.stencilAttachmentFormat = depth_format_;
        }
    }

    VkGraphicsPipelineCreateInfo const create_info{
        .sType = vku::GetSType<VkGraphicsPipelineCreateInfo>(),
        .pNext = &rendering_create_info,
        .stageCount = count_cast(shaders_.size()),
        .pStages = shaders_.data(),
        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &input_assembly,
        .pTessellationState =
            tesselation_ ? &tesselation_.value() : VK_NULL_HANDLE,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState =
            depth_stencil_ ? &depth_stencil_.value() : VK_NULL_HANDLE,
        .pColorBlendState = &color_blending,
        .pDynamicState = &dynamic_state,
        .layout = *layout_,
    };

    pipeline_t rv{*layout_, VK_NULL_HANDLE, VK_PIPELINE_BIND_POINT_GRAPHICS};
    check_result(vkCreateGraphicsPipelines(*device_,
        VK_NULL_HANDLE,
        1,
        &create_info,
        nullptr,
        &rv.handle));

    return rv;
}

vkrndr::graphics_pipeline_builder_t&
vkrndr::graphics_pipeline_builder_t::add_shader(
    VkPipelineShaderStageCreateInfo const shader)
{
    shaders_.push_back(shader);

    return *this;
}

vkrndr::graphics_pipeline_builder_t&
vkrndr::graphics_pipeline_builder_t::add_color_attachment(VkFormat const format,
    std::optional<VkPipelineColorBlendAttachmentState> const& color_blending)
{
    static constexpr VkPipelineColorBlendAttachmentState const
        default_color_blending{.blendEnable = VK_FALSE,
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT |
                VK_COLOR_COMPONENT_A_BIT};

    color_attachments_.push_back(format);
    color_blending_.push_back(color_blending.value_or(default_color_blending));

    return *this;
}

vkrndr::graphics_pipeline_builder_t&
vkrndr::graphics_pipeline_builder_t::add_vertex_input(
    std::span<VkVertexInputBindingDescription const> const&
        binding_descriptions,
    std::span<VkVertexInputAttributeDescription const> const&
        attribute_descriptions)
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

vkrndr::graphics_pipeline_builder_t&
vkrndr::graphics_pipeline_builder_t::with_rasterization_samples(
    VkSampleCountFlagBits const samples)
{
    rasterization_samples_ = samples;
    return *this;
}

vkrndr::graphics_pipeline_builder_t&
vkrndr::graphics_pipeline_builder_t::with_culling(
    VkCullModeFlags const cull_mode,
    VkFrontFace const front_face)
{
    cull_mode_ = cull_mode;
    front_face_ = front_face;

    return *this;
}

vkrndr::graphics_pipeline_builder_t&
vkrndr::graphics_pipeline_builder_t::with_primitive_topology(
    VkPrimitiveTopology const primitive_topology)
{
    primitive_topology_ = primitive_topology;

    return *this;
}

vkrndr::graphics_pipeline_builder_t&
vkrndr::graphics_pipeline_builder_t::with_depth_test(
    VkFormat const depth_format,
    VkCompareOp const compare,
    bool const write)
{
    assert(
        depth_format_ == VK_FORMAT_UNDEFINED || depth_format_ == depth_format);

    depth_format_ = depth_format;

    VkPipelineDepthStencilStateCreateInfo depth_stencil{
        depth_stencil_.value_or(VkPipelineDepthStencilStateCreateInfo{
            .sType = vku::GetSType<VkPipelineDepthStencilStateCreateInfo>(),
            .minDepthBounds = 0.0f,
            .maxDepthBounds = 1.0f,
        })};

    auto const enabled{
        depth_format_ != VK_FORMAT_UNDEFINED ? VK_TRUE : VK_FALSE};

    depth_stencil.depthTestEnable = enabled;
    depth_stencil.depthWriteEnable = static_cast<VkBool32>(write);
    depth_stencil.depthCompareOp = compare;

    depth_stencil_ = depth_stencil;

    return *this;
}

vkrndr::graphics_pipeline_builder_t&
vkrndr::graphics_pipeline_builder_t::with_stencil_test(
    VkFormat const depth_format,
    VkStencilOpState const front,
    VkStencilOpState const back)
{
    assert(
        depth_format_ == VK_FORMAT_UNDEFINED || depth_format_ == depth_format);

    depth_format_ = depth_format;

    VkPipelineDepthStencilStateCreateInfo depth_stencil{
        depth_stencil_.value_or(VkPipelineDepthStencilStateCreateInfo{
            .sType = vku::GetSType<VkPipelineDepthStencilStateCreateInfo>(),
            .depthCompareOp = VK_COMPARE_OP_LESS,
            .minDepthBounds = 0.0f,
            .maxDepthBounds = 1.0f,
        })};

    auto const enabled{
        depth_format_ != VK_FORMAT_UNDEFINED ? VK_TRUE : VK_FALSE};
    depth_stencil.stencilTestEnable = enabled;
    depth_stencil.front = front;
    depth_stencil.back = back;

    depth_stencil_ = depth_stencil;

    return *this;
}

vkrndr::graphics_pipeline_builder_t&
vkrndr::graphics_pipeline_builder_t::with_tesselation_patch_points(
    uint32_t const points)
{
    tesselation_ = {
        .sType = vku::GetSType<VkPipelineTessellationStateCreateInfo>(),
        .patchControlPoints = points,
    };

    return *this;
}

vkrndr::graphics_pipeline_builder_t&
vkrndr::graphics_pipeline_builder_t::with_dynamic_state(VkDynamicState state)
{
    if (std::ranges::find(dynamic_states_, state) == std::cend(dynamic_states_))
    {
        dynamic_states_.push_back(state);
    }
    return *this;
}
