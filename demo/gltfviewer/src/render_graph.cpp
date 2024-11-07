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
#include <glm/matrix.hpp>

#include <array>
#include <cstddef>
#include <functional>
#include <utility>

// IWYU pragma: no_include <ranges>
// IWYU pragma: no_include <vector>

namespace
{
    struct [[nodiscard]] push_constants_t final
    {
        uint32_t transform_index;
        uint32_t material_index;
    };

    struct [[nodiscard]] transform_t final
    {
        glm::mat4 model;
        glm::mat4 model_inverse;
    };

    [[nodiscard]] VkDescriptorSetLayout create_descriptor_set_layout(
        vkrndr::device_t const& device)
    {
        VkDescriptorSetLayoutBinding transform_uniform_binding{};
        transform_uniform_binding.binding = 0;
        transform_uniform_binding.descriptorType =
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        transform_uniform_binding.descriptorCount = 1;
        transform_uniform_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        std::array const bindings{transform_uniform_binding};

        VkDescriptorSetLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = vkrndr::count_cast(bindings.size());
        layout_info.pBindings = bindings.data();

        VkDescriptorSetLayout rv; // NOLINT
        vkrndr::check_result(vkCreateDescriptorSetLayout(device.logical,
            &layout_info,
            nullptr,
            &rv));

        return rv;
    }

    void update_descriptor_set(vkrndr::device_t const& device,
        VkDescriptorSet const descriptor_set,
        VkDescriptorBufferInfo const transform_uniform_info)
    {
        VkWriteDescriptorSet transform_storage_write{};
        transform_storage_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        transform_storage_write.dstSet = descriptor_set;
        transform_storage_write.dstBinding = 0;
        transform_storage_write.dstArrayElement = 0;
        transform_storage_write.descriptorType =
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        transform_storage_write.descriptorCount = 1;
        transform_storage_write.pBufferInfo = &transform_uniform_info;

        std::array const descriptor_writes{transform_storage_write};

        vkUpdateDescriptorSets(device.logical,
            vkrndr::count_cast(descriptor_writes.size()),
            descriptor_writes.data(),
            0,
            nullptr);
    }

    [[nodiscard]] uint32_t nodes_with_mesh(vkgltf::node_t const& node,
        vkgltf::model_t const& model)
    {
        auto rv{static_cast<uint32_t>(node.mesh != nullptr)};
        // cppcheck-suppress-begin useStlAlgorithm
        for (auto const& child : node.children(model))
        {
            rv += nodes_with_mesh(child, model);
        }
        // cppcheck-suppress-end useStlAlgorithm
        return rv;
    }

    [[nodiscard]] uint32_t required_transforms(vkgltf::model_t const& model)
    {
        uint32_t rv{};
        // cppcheck-suppress-begin useStlAlgorithm
        for (auto const& graph : model.scenes)
        {
            for (auto const& root : graph.roots(model))
            {
                rv += nodes_with_mesh(root, model);
            }
        }
        // cppcheck-suppress-end useStlAlgorithm
        return rv;
    }

    [[nodiscard]] uint32_t calculate_transform(vkgltf::model_t const& model,
        vkgltf::node_t const& node,
        transform_t* const transforms,
        glm::mat4 const& transform,
        uint32_t const index)
    {
        auto const node_transform{transform * node.matrix};

        uint32_t drawn{0};
        if (node.mesh)
        {
            transforms[index].model = node_transform;
            transforms[index].model_inverse =
                glm::transpose(glm::inverse(node_transform));

            ++drawn;
        }

        // cppcheck-suppress-begin useStlAlgorithm
        for (auto const& child : node.children(model))
        {
            drawn += calculate_transform(model,
                child,
                transforms,
                node_transform,
                index + drawn);
        }
        // cppcheck-suppress-end useStlAlgorithm

        return drawn;
    }
} // namespace

gltfviewer::render_graph_t::render_graph_t(vkrndr::backend_t& backend)
    : backend_{&backend}
    , frame_data_{backend_->frames_in_flight(), backend_->frames_in_flight()}
{
}

gltfviewer::render_graph_t::~render_graph_t() { clear(); }

vkgltf::model_t const& gltfviewer::render_graph_t::model() const
{
    return model_;
}

VkDescriptorSetLayout gltfviewer::render_graph_t::descriptor_layout() const
{
    return descriptor_layout_;
}

std::span<VkVertexInputBindingDescription const>
gltfviewer::render_graph_t::binding_description() const
{
    static constexpr std::array descriptions{
        VkVertexInputBindingDescription{.binding = 0,
            .stride = sizeof(vkgltf::vertex_t),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX},
    };

    return descriptions;
}

std::span<VkVertexInputAttributeDescription const>
gltfviewer::render_graph_t::attribute_description() const
{
    static constexpr std::array descriptions{
        VkVertexInputAttributeDescription{.location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(vkgltf::vertex_t, position)},
        VkVertexInputAttributeDescription{.location = 1,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(vkgltf::vertex_t, normal)},
        VkVertexInputAttributeDescription{.location = 2,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset = offsetof(vkgltf::vertex_t, tangent)},
        VkVertexInputAttributeDescription{.location = 3,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset = offsetof(vkgltf::vertex_t, color)},
        VkVertexInputAttributeDescription{.location = 4,
            .binding = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(vkgltf::vertex_t, uv)},
    };

    return descriptions;
}

void gltfviewer::render_graph_t::load(vkgltf::model_t&& model)
{
    clear();

    descriptor_layout_ = create_descriptor_set_layout(backend_->device());

    vertex_count_ = model.vertex_count;
    vertex_buffer_ = create_buffer(backend_->device(),
        model.vertex_buffer.size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    backend_->transfer_buffer(model.vertex_buffer, vertex_buffer_);

    if (model.index_count > 0)
    {
        index_count_ = model.index_count;
        index_buffer_ = create_buffer(backend_->device(),
            model.index_buffer.size,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        backend_->transfer_buffer(model.index_buffer, index_buffer_);
    }

    uint32_t const transform_count{required_transforms(model)};
    VkDeviceSize const transform_buffer_size{
        transform_count * sizeof(transform_t)};
    for (auto& data : frame_data_.as_span())
    {
        data.uniform = create_buffer(backend_->device(),
            transform_buffer_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        data.uniform_map = map_memory(backend_->device(), data.uniform);

        vkrndr::create_descriptor_sets(backend_->device(),
            backend_->descriptor_pool(),
            std::span{&descriptor_layout_, 1},
            std::span{&data.descriptor_set, 1});

        update_descriptor_set(backend_->device(),
            data.descriptor_set,
            vkrndr::buffer_descriptor(data.uniform));
    }

    calculate_transforms(model, frame_data_.as_span());

    model_ = std::move(model);
}

void gltfviewer::render_graph_t::bind_on(VkCommandBuffer command_buffer,
    VkPipelineLayout layout,
    VkPipelineBindPoint const bind_point)
{
    frame_data_.cycle();

    vkCmdBindDescriptorSets(command_buffer,
        bind_point,
        layout,
        2,
        1,
        &frame_data_->descriptor_set,
        0,
        nullptr);

    if (vertex_count_)
    {
        VkDeviceSize const zero_offset{};
        vkCmdBindVertexBuffers(command_buffer,
            0,
            1,
            &vertex_buffer_.buffer,
            &zero_offset);
    }

    if (index_count_)
    {
        vkCmdBindIndexBuffer(command_buffer,
            index_buffer_.buffer,
            0,
            VK_INDEX_TYPE_UINT32);
    }
}

void gltfviewer::render_graph_t::traverse(vkgltf::alpha_mode_t alpha_mode,
    VkCommandBuffer command_buffer,
    VkPipelineLayout layout,
    std::function<void(vkgltf::alpha_mode_t, bool)> const& switch_pipeline)
    const
{
    uint32_t drawn{0};
    for (auto const& graph : model_.scenes)
    {
        // cppcheck-suppress-begin useStlAlgorithm
        for (auto const& root : graph.roots(model_))
        {
            drawn += draw_node(command_buffer,
                layout,
                root,
                alpha_mode,
                drawn,
                switch_pipeline);
        }
        // cppcheck-suppress-end useStlAlgorithm
    }
}

void gltfviewer::render_graph_t::calculate_transforms(
    vkgltf::model_t const& model,
    std::span<frame_data_t> frames)
{
    for (frame_data_t& data : frames)
    {
        uint32_t drawn{0};
        for (auto const& graph : model.scenes)
        {
            // cppcheck-suppress-begin useStlAlgorithm
            for (auto const& root : graph.roots(model))
            {
                drawn += calculate_transform(model,
                    root,
                    data.uniform_map.as<transform_t>(),
                    glm::mat4{1.0f},
                    drawn);
            }
            // cppcheck-suppress-end useStlAlgorithm
        }
    }
}

uint32_t gltfviewer::render_graph_t::draw_node(VkCommandBuffer command_buffer,
    VkPipelineLayout layout,
    vkgltf::node_t const& node,
    vkgltf::alpha_mode_t const& alpha_mode,
    uint32_t const index,
    std::function<void(vkgltf::alpha_mode_t, bool)> const& switch_pipeline)
    const
{
    uint32_t drawn{0};
    if (node.mesh)
    {
        // cppcheck-suppress-begin useStlAlgorithm
        for (auto const& primitive : node.mesh->primitives)
        {
            push_constants_t const pc{.transform_index = index,
                .material_index =
                    cppext::narrow<uint32_t>(primitive.material_index)};

            if (!model_.materials.empty())
            {
                auto const& material{
                    model_.materials[primitive.material_index]};
                if (material.alpha_mode != alpha_mode)
                {
                    continue;
                }
                switch_pipeline(alpha_mode, material.double_sided);
            }

            vkCmdPushConstants(command_buffer,
                layout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                8,
                sizeof(pc),
                &pc);
            if (primitive.is_indexed)
            {
                vkCmdDrawIndexed(command_buffer,
                    primitive.count,
                    1,
                    primitive.first,
                    primitive.vertex_offset,
                    0);
            }
            else
            {
                vkCmdDraw(command_buffer,
                    primitive.count,
                    1,
                    primitive.first,
                    0);
            }
        }
        // cppcheck-suppress-end useStlAlgorithm

        ++drawn;
    }

    // cppcheck-suppress-begin useStlAlgorithm
    for (auto const& child : node.children(model_))
    {
        drawn += draw_node(command_buffer,
            layout,
            child,
            alpha_mode,
            index + drawn,
            switch_pipeline);
    }
    // cppcheck-suppress-end useStlAlgorithm

    return drawn;
}

void gltfviewer::render_graph_t::clear()
{
    for (frame_data_t& data : frame_data_.as_span())
    {
        free_descriptor_sets(backend_->device(),
            backend_->descriptor_pool(),
            std::span{&data.descriptor_set, 1});

        if (data.uniform_map.allocation)
        {
            unmap_memory(backend_->device(), &data.uniform_map);
            destroy(&backend_->device(), &data.uniform);
        }
    }

    if (index_count_ != 0)
    {
        destroy(&backend_->device(), &index_buffer_);
        index_count_ = 0;
    }

    if (vertex_count_ != 0)
    {
        destroy(&backend_->device(), &vertex_buffer_);
        vertex_count_ = 0;
    }

    destroy(&backend_->device(), &model_);

    vkDestroyDescriptorSetLayout(backend_->device().logical,
        descriptor_layout_,
        nullptr);
}
