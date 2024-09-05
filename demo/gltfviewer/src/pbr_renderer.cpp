#include "vkrndr_memory.hpp"
#include <algorithm>
#include <pbr_renderer.hpp>

#include <cppext_numeric.hpp>

#include <vkgltf_model.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_buffer.hpp>
#include <vkrndr_commands.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_pipeline.hpp>
#include <vkrndr_render_pass.hpp>

#include <glm/vec3.hpp>

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
} // namespace

gltfviewer::pbr_renderer_t::pbr_renderer_t(vkrndr::backend_t* const backend)
    : backend_{backend}
    , pipeline_{
          vkrndr::pipeline_builder_t{&backend_->device(),
              vkrndr::pipeline_layout_builder_t{&backend_->device()}.build(),
              backend_->image_format()}
              .add_shader(VK_SHADER_STAGE_VERTEX_BIT, "pbr.vert.spv", "main")
              .add_shader(VK_SHADER_STAGE_FRAGMENT_BIT, "pbr.frag.spv", "main")
              .with_primitive_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
              .with_rasterization_samples(VK_SAMPLE_COUNT_1_BIT)
              .add_vertex_input(binding_description(), attribute_descriptions())
              .build()}
{
}

gltfviewer::pbr_renderer_t::~pbr_renderer_t()
{
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

        vkrndr::bind_pipeline(command_buffer, pipeline_);

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
