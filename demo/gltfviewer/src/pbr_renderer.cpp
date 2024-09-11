#include <pbr_renderer.hpp>

#include <cppext_cycled_buffer.hpp>
#include <cppext_numeric.hpp>

#include <niku_camera.hpp>

#include <vkgltf_model.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_buffer.hpp>
#include <vkrndr_commands.hpp>
#include <vkrndr_depth_buffer.hpp>
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
    consteval auto binding_description()
    {
        constexpr std::array descriptions{
            VkVertexInputBindingDescription{.binding = 0,
                .stride = sizeof(vkgltf::vertex_t),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX}};

        return descriptions;
    }

    consteval auto attribute_descriptions()
    {
        constexpr std::array descriptions{
            VkVertexInputAttributeDescription{.location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(vkgltf::vertex_t, position)}};

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
    , depth_buffer_{vkrndr::create_depth_buffer(backend_->device(),
          backend_->extent(),
          false,
          VK_SAMPLE_COUNT_1_BIT)}
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
              .with_depth_test(depth_buffer_.format)
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
            std::span{&data.camera_descriptor_set, 1});

        bind_descriptor_set(backend_->device(),
            data.camera_descriptor_set,
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
            std::span{&data.camera_descriptor_set, 1});
    }

    vkDestroyDescriptorSetLayout(backend_->device().logical,
        descriptor_set_layout_,
        nullptr);

    destroy(&backend_->device(), &pipeline_);
    destroy(&backend_->device(), &model_);
    destroy(&backend_->device(), &depth_buffer_);
}

void gltfviewer::pbr_renderer_t::load_model(vkgltf::model_t&& model)
{
    auto real_vertex_buffer{create_buffer(backend_->device(),
        model.vertex_buffer.size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};
    auto real_index_buffer{create_buffer(backend_->device(),
        model.index_buffer.size,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};

    backend_->transfer_buffer(model.vertex_buffer, real_vertex_buffer);
    backend_->transfer_buffer(model.index_buffer, real_index_buffer);

    destroy(&backend_->device(), &model.vertex_buffer);
    destroy(&backend_->device(), &model.index_buffer);

    model_ = std::move(model);
    model_.vertex_buffer = real_vertex_buffer;
    model_.index_buffer = real_index_buffer;
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
        color_render_pass.with_depth_attachment(VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            depth_buffer_.view,
            VkClearValue{.depthStencil = {1.0f, 0}});

        [[maybe_unused]] auto guard{color_render_pass.begin(command_buffer,
            {{0, 0}, target_image.extent})};

        if (!model_.vertex_count)
        {
            return;
        }

        vkrndr::bind_pipeline(command_buffer,
            pipeline_,
            0,
            std::span{&frame_data_->camera_descriptor_set, 1});

        VkDeviceSize const zero_offset{};
        vkCmdBindVertexBuffers(command_buffer,
            0,
            1,
            &model_.vertex_buffer.buffer,
            &zero_offset);

        vkCmdBindIndexBuffer(command_buffer,
            model_.index_buffer.buffer,
            0,
            VK_INDEX_TYPE_UINT32);

        for (auto const& mesh : model_.meshes)
        {
            for (auto const& primitive : mesh.primitives)
            {
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
        }
    }
}

void gltfviewer::pbr_renderer_t::resize(uint32_t width, uint32_t height)
{
    destroy(&backend_->device(), &depth_buffer_);
    depth_buffer_ = vkrndr::create_depth_buffer(backend_->device(),
        VkExtent2D{width, height},
        false,
        VK_SAMPLE_COUNT_1_BIT);
}
