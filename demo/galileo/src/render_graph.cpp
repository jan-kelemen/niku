#include <render_graph.hpp>

#include <cppext_cycled_buffer.hpp>
#include <cppext_numeric.hpp>

#include <vkgltf_model.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_buffer.hpp>
#include <vkrndr_descriptors.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_memory.hpp>
#include <vkrndr_utility.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <volk.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

// IWYU pragma: no_include <vector>

namespace
{
    struct [[nodiscard]] line_vertex_t final
    {
        glm::vec3 position;
        char padding;
        glm::vec4 color;
    };

    struct [[nodiscard]] gpu_render_graph_t final
    {
        glm::mat4 position;
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
} // namespace

std::span<VkVertexInputBindingDescription const>
galileo::render_graph_t::binding_description()
{
    static constexpr std::array descriptions{
        VkVertexInputBindingDescription{.binding = 0,
            .stride = sizeof(line_vertex_t),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX},
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
            .offset = offsetof(line_vertex_t, position)},
        VkVertexInputAttributeDescription{.location = 1,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset = offsetof(line_vertex_t, color)},
    };

    return descriptions;
}

galileo::render_graph_t::render_graph_t(vkrndr::backend_t& backend)
    : backend_{&backend}
    , descriptor_set_layout_{create_descriptor_set_layout(backend_->device())}
    , frame_data_{backend_->frames_in_flight(), backend_->frames_in_flight()}
{
    for (auto& data : frame_data_.as_span())
    {
        vkrndr::create_descriptor_sets(backend_->device(),
            backend_->descriptor_pool(),
            std::span{&descriptor_set_layout_, 1},
            std::span{&data.descriptor_set, 1});
    }
}

galileo::render_graph_t::~render_graph_t()
{
    for (auto& data : frame_data_.as_span())
    {
        vkrndr::free_descriptor_sets(backend_->device(),
            backend_->descriptor_pool(),
            std::span{&data.descriptor_set, 1});

        vkrndr::unmap_memory(backend_->device(), &data.uniform_map);

        vkrndr::destroy(&backend_->device(), &data.uniform);
    }

    destroy(&backend_->device(), &model_);

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

void galileo::render_graph_t::load(vkgltf::model_t&& model)
{
    destroy(&backend_->device(), &vertex_buffer_);
    vertex_buffer_ = vkrndr::create_buffer(backend_->device(),
        sizeof(line_vertex_t) *
            (model.vertex_buffer.size / sizeof(vkgltf::vertex_t)),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    {
        auto staging{vkrndr::create_staging_buffer(backend_->device(),
            vertex_buffer_.size)};
        auto staging_map{vkrndr::map_memory(backend_->device(), staging)};
        auto* const vertices{staging_map.as<line_vertex_t>()};

        auto gltf_map{
            vkrndr::map_memory(backend_->device(), model.vertex_buffer)};
        auto* const gltf_vertices{gltf_map.as<vkgltf::vertex_t>()};

        for (uint32_t i{}; i != model.vertex_count; ++i)
        {
            vertices[i].position = gltf_vertices[i].position;
            vertices[i].color = gltf_vertices[i].color;
        }

        unmap_memory(backend_->device(), &gltf_map);
        unmap_memory(backend_->device(), &staging_map);

        backend_->transfer_buffer(staging, vertex_buffer_);
        destroy(&backend_->device(), &staging);
    }

    destroy(&backend_->device(), &index_buffer_);
    index_buffer_ = vkrndr::create_buffer(backend_->device(),
        model.index_buffer.size,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    backend_->transfer_buffer(model.index_buffer, index_buffer_);

    for (auto& data : frame_data_.as_span())
    {
        if (data.uniform_map.mapped_memory)
        {
            vkrndr::unmap_memory(backend_->device(), &data.uniform_map);

            vkrndr::destroy(&backend_->device(), &data.uniform);
        }

        data.uniform = vkrndr::create_buffer(backend_->device(),
            sizeof(gpu_render_graph_t) * model.nodes.size(),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        data.uniform_map = vkrndr::map_memory(backend_->device(), data.uniform);

        update_descriptor_set(backend_->device(),
            data.descriptor_set,
            vkrndr::buffer_descriptor(data.uniform));
    }

    destroy(&backend_->device(), &model_);
    model_ = std::move(model);
}

void galileo::render_graph_t::begin_frame() { frame_data_.cycle(); }

void galileo::render_graph_t::update(size_t const index,
    glm::mat4 const& position)
{
    auto* const gpu{frame_data_->uniform_map.as<gpu_render_graph_t>()};
    gpu[index].position = position;
}

void galileo::render_graph_t::bind_on(VkCommandBuffer command_buffer,
    VkPipelineLayout layout,
    VkPipelineBindPoint const bind_point)
{
    vkCmdBindDescriptorSets(command_buffer,
        bind_point,
        layout,
        1,
        1,
        &frame_data_->descriptor_set,
        0,
        nullptr);

    VkDeviceSize const zero_offset{};
    vkCmdBindVertexBuffers(command_buffer,
        0,
        1,
        &vertex_buffer_.buffer,
        &zero_offset);

    vkCmdBindIndexBuffer(command_buffer,
        index_buffer_.buffer,
        0,
        VK_INDEX_TYPE_UINT32);
}

void galileo::render_graph_t::draw_node(size_t const index,
    VkCommandBuffer command_buffer) const
{
    auto const& node{model_.nodes[index]};
    auto const& primitive{node.mesh->primitives.front()};

    if (primitive.is_indexed)
    {
        vkCmdDrawIndexed(command_buffer,
            primitive.count,
            1,
            primitive.first,
            primitive.vertex_offset,
            cppext::narrow<uint32_t>(index));
    }
    else
    {
        vkCmdDraw(command_buffer,
            primitive.count,
            1,
            primitive.first,
            cppext::narrow<uint32_t>(index));
    }
}