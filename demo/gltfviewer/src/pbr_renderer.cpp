#include <pbr_renderer.hpp>

#include <cppext_cycled_buffer.hpp>
#include <cppext_numeric.hpp>

#include <niku_camera.hpp>

#include <vkgltf_model.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_buffer.hpp>
#include <vkrndr_commands.hpp>
#include <vkrndr_descriptors.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_memory.hpp>
#include <vkrndr_pipeline.hpp>
#include <vkrndr_render_pass.hpp>
#include <vkrndr_utility.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <array>
#include <ranges>

namespace
{
    struct [[nodiscard]] vertex_t final
    {
        glm::vec3 position;
    };

    consteval auto binding_description()
    {
        constexpr std::array descriptions{
            VkVertexInputBindingDescription{.binding = 0,
                .stride = sizeof(vertex_t),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX}};

        return descriptions;
    }

    consteval auto attribute_descriptions()
    {
        constexpr std::array descriptions{
            VkVertexInputAttributeDescription{.location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(vertex_t, position)}};

        return descriptions;
    }

    struct [[nodiscard]] camera_uniform_t final
    {
        glm::mat4 view;
        glm::mat4 projection;
    };

    [[nodiscard]] VkDescriptorSetLayout create_descriptor_set_layout(
        vkrndr::device_t const& device)
    {
        VkDescriptorSetLayoutBinding camera_uniform_binding{};
        camera_uniform_binding.binding = 0;
        camera_uniform_binding.descriptorType =
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        camera_uniform_binding.descriptorCount = 1;
        camera_uniform_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        std::array const bindings{camera_uniform_binding};

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

    void bind_descriptor_set(vkrndr::device_t const& device,
        VkDescriptorSet const descriptor_set,
        VkDescriptorBufferInfo const camera_uniform_info)
    {
        VkWriteDescriptorSet camera_uniform_write{};
        camera_uniform_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        camera_uniform_write.dstSet = descriptor_set;
        camera_uniform_write.dstBinding = 0;
        camera_uniform_write.dstArrayElement = 0;
        camera_uniform_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        camera_uniform_write.descriptorCount = 1;
        camera_uniform_write.pBufferInfo = &camera_uniform_info;

        std::array const descriptor_writes{camera_uniform_write};

        vkUpdateDescriptorSets(device.logical,
            vkrndr::count_cast(descriptor_writes.size()),
            descriptor_writes.data(),
            0,
            nullptr);
    }

} // namespace

gltfviewer::pbr_renderer_t::pbr_renderer_t(vkrndr::backend_t* const backend)
    : backend_{backend}
    , descriptor_set_layout_{create_descriptor_set_layout(backend_->device())}
    , pipeline_{
          vkrndr::pipeline_builder_t{&backend_->device(),
              vkrndr::pipeline_layout_builder_t{&backend_->device()}
                  .add_descriptor_set_layout(descriptor_set_layout_)
                  .build(),
              backend_->image_format()}
              .add_shader(VK_SHADER_STAGE_VERTEX_BIT, "pbr.vert.spv", "main")
              .add_shader(VK_SHADER_STAGE_FRAGMENT_BIT, "pbr.frag.spv", "main")
              .with_primitive_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
              .with_rasterization_samples(VK_SAMPLE_COUNT_1_BIT)
              .add_vertex_input(binding_description(), attribute_descriptions())
              .build()}
{
    frame_data_ = cppext::cycled_buffer_t<frame_data_t>{backend_->image_count(),
        backend_->image_count()};

    for (auto& data : frame_data_.as_span())
    {
        auto const camera_uniform_buffer_size{sizeof(camera_uniform_t)};
        data.camera_uniform = create_buffer(backend_->device(),
            camera_uniform_buffer_size,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        data.camera_uniform_map =
            vkrndr::map_memory(backend_->device(), data.camera_uniform);

        vkrndr::create_descriptor_sets(backend_->device(),
            backend_->descriptor_pool(),
            std::span{&descriptor_set_layout_, 1},
            std::span{&data.descriptor_set, 1});

        bind_descriptor_set(backend_->device(),
            data.descriptor_set,
            VkDescriptorBufferInfo{.buffer = data.camera_uniform.buffer,
                .offset = 0,
                .range = camera_uniform_buffer_size});
    }
}

gltfviewer::pbr_renderer_t::~pbr_renderer_t()
{
    for (auto& data : frame_data_.as_span())
    {
        unmap_memory(backend_->device(), &data.camera_uniform_map);
        destroy(&backend_->device(), &data.camera_uniform);

        vkrndr::free_descriptor_sets(backend_->device(),
            backend_->descriptor_pool(),
            std::span{&data.descriptor_set, 1});
    }

    vkDestroyDescriptorSetLayout(backend_->device().logical,
        descriptor_set_layout_,
        nullptr);

    destroy(&backend_->device(), &pipeline_);
    destroy(&backend_->device(), &vertex_buffer_);
    destroy(&backend_->device(), &index_buffer_);
}

void gltfviewer::pbr_renderer_t::load_model(vkgltf::model_t const& model)
{
    uint32_t vertex_count{};
    uint32_t index_count{};

    auto const indexed = [](vkgltf::primitive_t const& primitive)
    { return primitive.indices.has_value(); };

    for (auto const& mesh : model.meshes)
    {
        // TODO-JK: Handle correctly mixture of indexed and nonindexed primitves
        for (auto const& primitive :
            mesh.primitives | std::views::filter(indexed))
        {
            vertex_count +=
                cppext::narrow<uint32_t>(primitive.positions.size());

            index_count += primitive.indices->count();
        }
    }

    vkrndr::buffer_t vtx_staging_buffer{create_buffer(backend_->device(),
        vertex_count * sizeof(vertex_t),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};
    vkrndr::buffer_t idx_staging_buffer{create_buffer(backend_->device(),
        index_count * sizeof(uint32_t),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};
    {
        auto vtx_map{
            vkrndr::map_memory(backend_->device(), vtx_staging_buffer)};
        auto idx_map{
            vkrndr::map_memory(backend_->device(), idx_staging_buffer)};

        vertex_t* vertices{vtx_map.as<vertex_t>()};
        uint32_t* indices{idx_map.as<uint32_t>()};

        for (auto const& mesh : model.meshes)
        {
            for (auto const& primitive :
                mesh.primitives | std::views::filter(indexed))
            {
                vertices = std::ranges::transform(primitive.positions,
                    vertices,
                    [](glm::vec3 const& pos) {
                        return vertex_t{.position = pos};
                    }).out;
                indices = std::visit(
                    [&indices](auto const& buff) mutable
                    {
                        return std::ranges::transform(buff,
                            indices,
                            [](uint32_t idx) { return idx; })
                            .out;
                    },
                    primitive.indices->buffer);
            }
        }

        vkrndr::unmap_memory(backend_->device(), &vtx_map);
        vkrndr::unmap_memory(backend_->device(), &idx_map);
    }

    vertex_buffer_ = create_buffer(backend_->device(),
        vtx_staging_buffer.size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    index_buffer_ = create_buffer(backend_->device(),
        idx_staging_buffer.size,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    backend_->transfer_buffer(vtx_staging_buffer, vertex_buffer_);
    backend_->transfer_buffer(idx_staging_buffer, index_buffer_);

    destroy(&backend_->device(), &vtx_staging_buffer);
    destroy(&backend_->device(), &idx_staging_buffer);

    vertex_count_ = vertex_count;
    index_count_ = index_count;
}

void gltfviewer::pbr_renderer_t::update(niku::camera_t const& camera)
{
    frame_data_.cycle();

    auto* const camera_uniform{
        frame_data_->camera_uniform_map.as<camera_uniform_t>()};
    camera_uniform->view = camera.view_matrix();
    camera_uniform->projection = camera.projection_matrix();
}

void gltfviewer::pbr_renderer_t::draw(VkCommandBuffer command_buffer,
    vkrndr::image_t const& target_image)
{
    vkrndr::transition_image(target_image.image,
        command_buffer,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_PIPELINE_STAGE_2_BLIT_BIT,
        VK_ACCESS_2_TRANSFER_READ_BIT,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        1);

    {
        vkrndr::render_pass_t color_render_pass;
        color_render_pass.with_color_attachment(VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            target_image.view,
            VkClearValue{.color = {{1.0f, 0.5f, 0.5f}}});

        [[maybe_unused]] auto guard{color_render_pass.begin(command_buffer,
            {{0, 0}, target_image.extent})};

        if (!vertex_count_)
        {
            return;
        }

        vkrndr::bind_pipeline(command_buffer,
            pipeline_,
            0,
            std::span{&frame_data_->descriptor_set, 1});

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

        vkCmdDrawIndexed(command_buffer, index_count_, 1, 0, 0, 0);
    }
}
