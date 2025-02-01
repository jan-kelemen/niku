#include <render_graph.hpp>

#include <cppext_container.hpp>
#include <cppext_cycled_buffer.hpp>
#include <cppext_numeric.hpp>

#include <ngnast_gpu_transfer.hpp>
#include <ngnast_scene_model.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_buffer.hpp>
#include <vkrndr_descriptors.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_memory.hpp>
#include <vkrndr_utility.hpp>

#include <boost/scope/defer.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <vma_impl.hpp>

#include <volk.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <ranges>
#include <span>

// IWYU pragma: no_include <optional>
// IWYU pragma: no_include <string>
// IWYU pragma: no_include <vector>

namespace
{
    constexpr size_t max_instance_count{1000};

    struct [[nodiscard]] graph_vertex_t final
    {
        glm::vec3 position;
        char padding1;
        glm::vec3 normal;
        char padding2;
        glm::vec4 color;
        glm::vec2 uv;
    };

    struct [[nodiscard]] graph_instance_vertex_t final
    {
        uint32_t primitive;
        uint32_t index;
    };

    struct [[nodiscard]] gpu_render_node_t final
    {
        glm::mat4 position;
        uint32_t material;
        uint8_t padding[12];
    };

    [[nodiscard]] VkDescriptorSetLayout create_descriptor_set_layout(
        vkrndr::device_t const& device)
    {
        VkDescriptorSetLayoutBinding uniform_binding{};
        uniform_binding.binding = 0;
        uniform_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        uniform_binding.descriptorCount = 1;
        uniform_binding.stageFlags =
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array bindings{uniform_binding};

        return vkrndr::create_descriptor_set_layout(device, bindings);
    }

    void update_descriptor_set(vkrndr::device_t const& device,
        VkDescriptorSet const descriptor_set,
        VkDescriptorBufferInfo const uniform_info)
    {
        VkWriteDescriptorSet uniform_write{};
        uniform_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        uniform_write.dstSet = descriptor_set;
        uniform_write.dstBinding = 0;
        uniform_write.dstArrayElement = 0;
        uniform_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        uniform_write.descriptorCount = 1;
        uniform_write.pBufferInfo = &uniform_info;

        std::array const descriptor_writes{uniform_write};

        vkUpdateDescriptorSets(device.logical,
            vkrndr::count_cast(descriptor_writes.size()),
            descriptor_writes.data(),
            0,
            nullptr);
    }

    galileo::render_node_t to_render_node(ngnast::node_t const& n)
    {
        return {.matrix = n.matrix,
            .mesh_index = n.mesh_index,
            .child_indices = n.child_indices};
    }
} // namespace

std::span<VkVertexInputBindingDescription const>
galileo::render_graph_t::binding_description()
{
    static constexpr std::array descriptions{
        VkVertexInputBindingDescription{.binding = 0,
            .stride = sizeof(graph_vertex_t),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX},
        VkVertexInputBindingDescription{.binding = 1,
            .stride = sizeof(graph_instance_vertex_t),
            .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE},
    };

    return descriptions;
}

std::span<VkVertexInputAttributeDescription const>
galileo::render_graph_t::attribute_description()
{
    static constexpr std::array descriptions{
        VkVertexInputAttributeDescription{.location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(graph_vertex_t, position)},
        VkVertexInputAttributeDescription{.location = 1,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(graph_vertex_t, normal)},
        VkVertexInputAttributeDescription{.location = 2,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset = offsetof(graph_vertex_t, color)},
        VkVertexInputAttributeDescription{.location = 3,
            .binding = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(graph_vertex_t, uv)},
        VkVertexInputAttributeDescription{.location = 4,
            .binding = 1,
            .format = VK_FORMAT_R32_UINT,
            .offset = offsetof(graph_instance_vertex_t, index)},
    };

    return descriptions;
}

galileo::render_graph_t::render_graph_t(vkrndr::backend_t& backend)
    : backend_{&backend}
    , descriptor_set_layout_{create_descriptor_set_layout(backend_->device())}
    , frame_data_{backend_->frames_in_flight(), backend_->frames_in_flight()}
{
    for (auto& data : cppext::as_span(frame_data_))
    {
        data.instance_vertex_buffer = vkrndr::create_buffer(backend_->device(),
            {.size = sizeof(graph_instance_vertex_t) * max_instance_count,
                .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                .allocation_flags =
                    VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
                .required_memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT});

        data.instance_map =
            vkrndr::map_memory(backend_->device(), data.instance_vertex_buffer);

        data.uniform = vkrndr::create_buffer(backend_->device(),
            {.size = sizeof(gpu_render_node_t) * max_instance_count,
                .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                .allocation_flags =
                    VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
                .required_memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT});

        data.uniform_map = vkrndr::map_memory(backend_->device(), data.uniform);

        vkrndr::create_descriptor_sets(backend_->device(),
            backend_->descriptor_pool(),
            cppext::as_span(descriptor_set_layout_),
            cppext::as_span(data.descriptor_set));

        update_descriptor_set(backend_->device(),
            data.descriptor_set,
            vkrndr::buffer_descriptor(data.uniform));
    }
}

galileo::render_graph_t::~render_graph_t()
{
    for (auto& data : cppext::as_span(frame_data_))
    {
        vkrndr::free_descriptor_sets(backend_->device(),
            backend_->descriptor_pool(),
            cppext::as_span(data.descriptor_set));

        vkrndr::unmap_memory(backend_->device(), &data.uniform_map);

        vkrndr::destroy(&backend_->device(), &data.uniform);

        vkrndr::unmap_memory(backend_->device(), &data.instance_map);

        vkrndr::destroy(&backend_->device(), &data.instance_vertex_buffer);
    }

    destroy(&backend_->device(), &index_buffer_);

    destroy(&backend_->device(), &vertex_buffer_);

    vkDestroyDescriptorSetLayout(backend_->device().logical,
        descriptor_set_layout_,
        nullptr);
}

VkDescriptorSetLayout galileo::render_graph_t::descriptor_set_layout() const
{
    return descriptor_set_layout_;
}

void galileo::render_graph_t::consume(ngnast::scene_model_t& model)
{
    nodes_.clear();
    meshes_.clear();
    primitives_.clear();

    auto transfer_result{
        ngnast::gpu::transfer_geometry(backend_->device(), model)};
    [[maybe_unused]] boost::scope::defer_guard const destroy_transfer{
        [this, tr = &transfer_result]() { destroy(&backend_->device(), tr); }};

    for (ngnast::mesh_t const& mesh : model.meshes)
    {
        auto const first{primitives_.size()};
        for (auto const pi : mesh.primitive_indices)
        {
            auto const& gp{transfer_result.primitives[pi]};

            primitives_.emplace_back(0,
                gp.topology,
                gp.count,
                gp.first,
                gp.is_indexed,
                gp.vertex_offset,
                gp.material_index);
        }

        meshes_.emplace_back(first, mesh.primitive_indices.size());
    }
    std::ranges::transform(model.nodes,
        std::back_inserter(nodes_),
        to_render_node);

    destroy(&backend_->device(), &vertex_buffer_);
    vertex_buffer_ = vkrndr::create_buffer(backend_->device(),
        {.size = sizeof(graph_vertex_t) *
                (transfer_result.vertex_buffer.size /
                    sizeof(ngnast::gpu::vertex_t)),
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .required_memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT});
    {
        auto staging{vkrndr::create_staging_buffer(backend_->device(),
            vertex_buffer_.size)};
        auto staging_map{vkrndr::map_memory(backend_->device(), staging)};
        auto* const vertices{staging_map.as<graph_vertex_t>()};

        auto gltf_map{vkrndr::map_memory(backend_->device(),
            transfer_result.vertex_buffer)};
        auto* const gltf_vertices{gltf_map.as<ngnast::gpu::vertex_t>()};

        for (uint32_t i{}; i != transfer_result.vertex_count; ++i)
        {
            vertices[i].position = gltf_vertices[i].position;
            vertices[i].normal = gltf_vertices[i].normal;
            vertices[i].color = gltf_vertices[i].color;
            vertices[i].uv = gltf_vertices[i].uv;
        }

        unmap_memory(backend_->device(), &gltf_map);
        unmap_memory(backend_->device(), &staging_map);

        backend_->transfer_buffer(staging, vertex_buffer_);
        destroy(&backend_->device(), &staging);
    }

    destroy(&backend_->device(), &index_buffer_);
    index_buffer_ = vkrndr::create_buffer(backend_->device(),
        {.size = transfer_result.index_buffer.size,
            .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .required_memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT});

    backend_->transfer_buffer(transfer_result.index_buffer, index_buffer_);
}

void galileo::render_graph_t::begin_frame()
{
    frame_data_.cycle(
        [](frame_data_t const&, frame_data_t& next) { next.current_draw = 0; });

    std::ranges::for_each(primitives_,
        [](render_primitive_t& p) { p.instance_count = 0; });
}

void galileo::render_graph_t::update(size_t const index,
    glm::mat4 const& position)
{
    auto* const gpu_instance{
        frame_data_->instance_map.as<graph_instance_vertex_t>()};

    auto* const gpu_uniform{frame_data_->uniform_map.as<gpu_render_node_t>()};

    auto const& node{nodes_[index]};

    if (node.mesh_index)
    {
        auto const& mesh{meshes_[*node.mesh_index]};

        auto const last{mesh.first_primitive + mesh.count};
        for (auto i{mesh.first_primitive}; i != last; ++i)
        {
            auto& primitive{primitives_[i]};

            gpu_instance[frame_data_->current_draw] = {
                .primitive = cppext::narrow<uint32_t>(i),
                .index = frame_data_->current_draw};

            gpu_uniform[frame_data_->current_draw].material =
                cppext::narrow<uint32_t>(primitive.material_index);
            gpu_uniform[frame_data_->current_draw].position = position;

            ++primitive.instance_count;
            ++frame_data_->current_draw;
        }
    }

    glm::mat4 const new_position{position * node.matrix};
    for (auto const child : node.child_indices)
    {
        update(child, new_position);
    }
}

void galileo::render_graph_t::bind_on(VkCommandBuffer command_buffer,
    VkPipelineLayout layout,
    VkPipelineBindPoint const bind_point)
{
    vkCmdBindDescriptorSets(command_buffer,
        bind_point,
        layout,
        2,
        1,
        &frame_data_->descriptor_set,
        0,
        nullptr);

    std::array<VkBuffer, 2> const vertex_buffers{vertex_buffer_.buffer,
        frame_data_->instance_vertex_buffer.buffer};
    std::array<VkDeviceSize, 2> const vertex_offsets{0, 0};

    vkCmdBindVertexBuffers(command_buffer,
        0,
        2,
        vertex_buffers.data(),
        vertex_offsets.data());

    vkCmdBindIndexBuffer(command_buffer,
        index_buffer_.buffer,
        0,
        VK_INDEX_TYPE_UINT32);
}

void galileo::render_graph_t::draw(VkCommandBuffer command_buffer)
{
    auto* const instances{
        frame_data_->instance_map.as<graph_instance_vertex_t>()};

    std::span const instance_range{instances,
        instances + frame_data_->current_draw};
    std::ranges::sort(instance_range,
        std::less{},
        &graph_instance_vertex_t::primitive);

    uint32_t current_instance{};
    for (auto& primitive : primitives_ |
            std::views::filter(
                [](auto const& p) { return p.instance_count > 0; }))
    {
        if (primitive.is_indexed)
        {
            vkCmdDrawIndexed(command_buffer,
                primitive.count,
                primitive.instance_count,
                primitive.first,
                primitive.vertex_offset,
                current_instance);
        }
        else
        {
            vkCmdDraw(command_buffer,
                primitive.count,
                primitive.instance_count,
                primitive.first,
                current_instance);
        }

        current_instance += primitive.instance_count;
    }
}
