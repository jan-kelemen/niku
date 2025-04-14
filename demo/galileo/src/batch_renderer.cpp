#include <batch_renderer.hpp>

#include <config.hpp>

#include <cppext_cycled_buffer.hpp>

#include <vkglsl_shader_set.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_buffer.hpp>
#include <vkrndr_memory.hpp>
#include <vkrndr_pipeline.hpp>
#include <vkrndr_shader_module.hpp>

#include <vma_impl.hpp>

#include <boost/scope/defer.hpp>
#include <boost/scope/scope_fail.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <iterator>

// IWYU pragma: no_include <boost/scope/exception_checker.hpp>
// IWYU pragma: no_include <memory>
// IWYU pragma: no_include <filesystem>
// IWYU pragma: no_include <functional>
// IWYU pragma: no_include <expected>
// IWYU pragma: no_include <system_error>
// IWYU pragma: no_include <span>
// IWYU pragma: no_include <utility>

namespace
{
    consteval auto binding_description()
    {
        constexpr std::array descriptions{
            VkVertexInputBindingDescription{.binding = 0,
                .stride = sizeof(galileo::batch_vertex_t),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX},
        };

        return descriptions;
    }

    consteval auto attribute_description()
    {
        constexpr std::array descriptions{
            VkVertexInputAttributeDescription{.location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(galileo::batch_vertex_t, position)},
            VkVertexInputAttributeDescription{.location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32_SFLOAT,
                .offset = offsetof(galileo::batch_vertex_t, width)},
            VkVertexInputAttributeDescription{.location = 2,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                .offset = offsetof(galileo::batch_vertex_t, color)},
        };

        return descriptions;
    }
} // namespace

galileo::batch_renderer_t::batch_renderer_t(vkrndr::backend_t& backend,
    VkDescriptorSetLayout frame_info_layout,
    VkFormat const depth_buffer_format)
    : backend_{&backend}
    , frame_data_{backend_->frames_in_flight(), backend_->frames_in_flight()}
{
    vkglsl::shader_set_t shader_set{enable_shader_debug_symbols,
        enable_shader_optimization};

    auto vertex_shader{add_shader_module_from_path(shader_set,
        backend_->device(),
        VK_SHADER_STAGE_VERTEX_BIT,
        "debug.vert")};
    assert(vertex_shader);
    [[maybe_unused]] boost::scope::defer_guard const destroy_vtx{
        [this, shd = &vertex_shader.value()]()
        { destroy(&backend_->device(), shd); }};

    auto fragment_shader{add_shader_module_from_path(shader_set,
        backend_->device(),
        VK_SHADER_STAGE_FRAGMENT_BIT,
        "debug.frag")};
    assert(fragment_shader);
    [[maybe_unused]] boost::scope::defer_guard const destroy_frag{
        [this, shd = &fragment_shader.value()]()
        { destroy(&backend_->device(), shd); }};

    VkPipelineColorBlendAttachmentState const alpha_blend{
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
            VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT,
    };

    triangle_pipeline_ =
        vkrndr::pipeline_builder_t{backend_->device(),
            vkrndr::pipeline_layout_builder_t{backend_->device()}
                .add_descriptor_set_layout(frame_info_layout)
                .build()}
            .add_shader(as_pipeline_shader(*vertex_shader))
            .add_shader(as_pipeline_shader(*fragment_shader))
            .add_color_attachment(VK_FORMAT_R16G16B16A16_SFLOAT, alpha_blend)
            .with_depth_test(depth_buffer_format,
                VK_COMPARE_OP_LESS_OR_EQUAL,
                false)
            .add_vertex_input(binding_description(), attribute_description())
            .build();

    line_pipeline_ =
        vkrndr::pipeline_builder_t{backend_->device(),
            triangle_pipeline_.layout}
            .with_primitive_topology(VK_PRIMITIVE_TOPOLOGY_LINE_LIST)
            .with_dynamic_state(VK_DYNAMIC_STATE_LINE_WIDTH)
            .add_shader(as_pipeline_shader(*vertex_shader))
            .add_shader(as_pipeline_shader(*fragment_shader))
            .add_color_attachment(VK_FORMAT_R16G16B16A16_SFLOAT, alpha_blend)
            .with_depth_test(depth_buffer_format,
                VK_COMPARE_OP_LESS_OR_EQUAL,
                false)
            .add_vertex_input(binding_description(), attribute_description())
            .build();

    point_pipeline_ =
        vkrndr::pipeline_builder_t{backend_->device(),
            triangle_pipeline_.layout}
            .with_primitive_topology(VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
            .add_shader(as_pipeline_shader(*vertex_shader))
            .add_shader(as_pipeline_shader(*fragment_shader))
            .add_color_attachment(VK_FORMAT_R16G16B16A16_SFLOAT, alpha_blend)
            .with_depth_test(depth_buffer_format,
                VK_COMPARE_OP_LESS_OR_EQUAL,
                false)
            .add_vertex_input(binding_description(), attribute_description())
            .build();
}

galileo::batch_renderer_t::~batch_renderer_t()
{
    destroy(&backend_->device(), &point_pipeline_);
    destroy(&backend_->device(), &line_pipeline_);
    destroy(&backend_->device(), &triangle_pipeline_);

    for (auto& data : cppext::as_span(frame_data_))
    {
        for (auto& buffer : data.buffers)
        {
            if (buffer.memory.allocation != VK_NULL_HANDLE)
            {
                vkrndr::unmap_memory(backend_->device(), &buffer.memory);
            }

            vkrndr::destroy(&backend_->device(), &buffer.buffer);
        }
    }
}

void galileo::batch_renderer_t::add_triangle(batch_vertex_t const& p1,
    batch_vertex_t const& p2,
    batch_vertex_t const& p3)
{
    vertex_buffer_t* const buffer{
        buffer_for(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)};
    batch_vertex_t* const mapping{
        buffer->memory.as<batch_vertex_t>() + buffer->size};

    mapping[0] = p1;
    mapping[1] = p2;
    mapping[2] = p3;

    buffer->size += 3;
}

void galileo::batch_renderer_t::add_line(batch_vertex_t const& p1,
    batch_vertex_t const& p2)
{
    vertex_buffer_t* const buffer{buffer_for(VK_PRIMITIVE_TOPOLOGY_LINE_LIST)};
    batch_vertex_t* const mapping{
        buffer->memory.as<batch_vertex_t>() + buffer->size};

    mapping[0] = p1;
    mapping[1] = p2;

    buffer->size += 2;
}

void galileo::batch_renderer_t::add_point(batch_vertex_t const& p1)
{
    vertex_buffer_t* const buffer{buffer_for(VK_PRIMITIVE_TOPOLOGY_POINT_LIST)};
    batch_vertex_t* const mapping{
        buffer->memory.as<batch_vertex_t>() + buffer->size};

    mapping[0] = p1;

    buffer->size += 1;
}

void galileo::batch_renderer_t::begin_frame()
{
    frame_data_.cycle(
        [this](frame_data_t const&, frame_data_t& next)
        {
            std::ranges::for_each(next.buffers,
                [this](vertex_buffer_t& buffer)
                {
                    buffer.size = 0;
                    if (buffer.memory.allocation != VK_NULL_HANDLE)
                    {
                        vkrndr::unmap_memory(backend_->device(),
                            &buffer.memory);
                        buffer.memory.allocation = VK_NULL_HANDLE;
                    }
                });
        });
}

VkPipelineLayout galileo::batch_renderer_t::pipeline_layout() const
{
    return *triangle_pipeline_.layout;
}

void galileo::batch_renderer_t::draw(VkCommandBuffer command_buffer)
{
    auto const draw_topology = [&command_buffer](
                                   std::vector<vertex_buffer_t> const& buffers,
                                   VkPrimitiveTopology const topology)
    {
        VkDeviceSize const zero_offset{};
        for (auto const& buffer : buffers)
        {
            if (buffer.topology != topology || buffer.size == 0)
            {
                continue;
            }

            vkCmdBindVertexBuffers(command_buffer,
                0,
                1,
                &buffer.buffer.buffer,
                &zero_offset);

            vkCmdDraw(command_buffer, buffer.size, 1, 0, 0);
        }
    };

    if (triangle_buffer_)
    {
        vkrndr::bind_pipeline(command_buffer, triangle_pipeline_);
        draw_topology(frame_data_->buffers,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    }

    if (line_buffer_)
    {
        vkrndr::bind_pipeline(command_buffer, line_pipeline_);
        vkCmdSetLineWidth(command_buffer, 3.0f);
        draw_topology(frame_data_->buffers, VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
    }

    if (point_buffer_)
    {
        vkrndr::bind_pipeline(command_buffer, point_pipeline_);
        draw_topology(frame_data_->buffers, VK_PRIMITIVE_TOPOLOGY_POINT_LIST);
    }

    triangle_buffer_.reset();
    line_buffer_.reset();
    point_buffer_.reset();
}

galileo::batch_renderer_t::vertex_buffer_t*
galileo::batch_renderer_t::buffer_for(VkPrimitiveTopology topology)
{
    auto from_cached_variable = [this, &topology](
                                    std::optional<size_t>& cached_index)
    {
        if (cached_index &&
            frame_data_->buffers[*cached_index].size !=
                frame_data_->buffers[*cached_index].capacity)
        {
            return &frame_data_->buffers[*cached_index];
        }

        auto existing{std::ranges::find_if(frame_data_->buffers,
            [&topology](vertex_buffer_t const& buffer)
            { return buffer.topology == topology && buffer.size == 0; })};
        if (existing != frame_data_->buffers.cend())
        {
            cached_index =
                std::distance(frame_data_->buffers.begin(), existing);
        }
        else
        {
            vkrndr::buffer_t buffer{create_buffer(backend_->device(),
                {.size = 30000 * sizeof(batch_vertex_t),
                    .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                    .allocation_flags =
                        VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
                    .required_memory_flags =
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT})};
            boost::scope::scope_fail f{
                [this, &buffer]() { destroy(&backend_->device(), &buffer); }};

            frame_data_->buffers.emplace_back(topology,
                buffer,
                vkrndr::mapped_memory_t{},
                0,
                30000);

            cached_index = frame_data_->buffers.size() - 1;

            f.set_active(false);
        }

        frame_data_->buffers[*cached_index].memory =
            vkrndr::map_memory(backend_->device(),
                frame_data_->buffers[*cached_index].buffer);
        return &frame_data_->buffers[*cached_index];
    };

    switch (topology)
    {
    case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
        return from_cached_variable(point_buffer_);
    case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
        return from_cached_variable(line_buffer_);
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
        return from_cached_variable(triangle_buffer_);
    default:
        assert(false);
    }

    return nullptr;
}
