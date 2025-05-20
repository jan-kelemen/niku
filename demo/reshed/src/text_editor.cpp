#include <text_editor.hpp>

#include <config.hpp>

#include <cppext_container.hpp>
#include <cppext_cycled_buffer.hpp>
#include <cppext_numeric.hpp>

#include <ngngfx_orthographic_projection.hpp>

#include <ngntxt_font_bitmap.hpp>
#include <ngntxt_font_face.hpp>
#include <ngntxt_shaping.hpp>

#include <vkglsl_shader_set.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_buffer.hpp>
#include <vkrndr_descriptors.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_memory.hpp>
#include <vkrndr_pipeline.hpp>
#include <vkrndr_shader_module.hpp>
#include <vkrndr_utility.hpp>

#include <boost/scope/defer.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

#include <hb.h>

#include <SDL3/SDL_events.h>

#include <vma_impl.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <map>
#include <utility>

// IWYU pragma: no_include <fmt/base.h>
// IWYU pragma: no_include <fmt/format.h>
// IWYU pragma: no_include <expected>
// IWYU pragma: no_include <filesystem>
// IWYU pragma: no_include <memory>
// IWYU pragma: no_include <system_error>
// IWYU pragma: no_include <span>

namespace
{
    struct [[nodiscard]] vertex_t final
    {
        glm::vec2 position;
        glm::vec2 uv;
    };

    constexpr auto binding_description()
    {
        static constexpr std::array descriptions{
            VkVertexInputBindingDescription{.binding = 0,
                .stride = sizeof(vertex_t),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX},
        };

        return descriptions;
    }

    constexpr auto attribute_description()
    {
        static constexpr std::array descriptions{
            VkVertexInputAttributeDescription{.location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(vertex_t, position)},
            VkVertexInputAttributeDescription{.location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(vertex_t, uv)}};

        return descriptions;
    }

    void update_descriptor_set(vkrndr::device_t const& device,
        VkDescriptorSet const& descriptor_set,
        VkDescriptorImageInfo const bitmap_sampler_info)
    {
        VkWriteDescriptorSet const texture_descriptor_write{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptor_set,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &bitmap_sampler_info};

        std::array const descriptor_writes{texture_descriptor_write};

        vkUpdateDescriptorSets(device.logical,
            vkrndr::count_cast(descriptor_writes.size()),
            descriptor_writes.data(),
            0,
            nullptr);
    }

    [[nodiscard]] VkSampler create_bitmap_sampler(
        vkrndr::device_t const& device)
    {
        VkPhysicalDeviceProperties properties; // NOLINT
        vkGetPhysicalDeviceProperties(device.physical, &properties);

        VkSamplerCreateInfo sampler_info{};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = VK_FILTER_NEAREST;
        sampler_info.minFilter = VK_FILTER_NEAREST;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.anisotropyEnable = VK_FALSE;
        sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        sampler_info.unnormalizedCoordinates = VK_FALSE;
        sampler_info.compareEnable = VK_FALSE;
        sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler_info.mipLodBias = 0.0f;
        sampler_info.minLod = 0.0f;
        sampler_info.maxLod = 1;

        VkSampler rv; // NOLINT
        vkrndr::check_result(
            vkCreateSampler(device.logical, &sampler_info, nullptr, &rv));

        return rv;
    }
} // namespace

reshed::text_editor_t::text_editor_t(vkrndr::backend_t& backend)
    : backend_{&backend}
    , buffer_{"Font w loadedg"}
    , shaping_buffer_{ngntxt::create_shaping_buffer()}
    , bitmap_sampler_{create_bitmap_sampler(backend_->device())}
    , frame_data_{backend_->frames_in_flight(), backend_->frames_in_flight()}
{
    vkglsl::shader_set_t shader_set{enable_shader_debug_symbols,
        enable_shader_optimization};

    auto vertex_shader{add_shader_module_from_path(shader_set,
        backend_->device(),
        VK_SHADER_STAGE_VERTEX_BIT,
        "text.vert")};
    assert(vertex_shader);
    [[maybe_unused]] boost::scope::defer_guard const destroy_vtx{
        [this, shd = &vertex_shader.value()]()
        { destroy(&backend_->device(), shd); }};

    auto fragment_shader{add_shader_module_from_path(shader_set,
        backend_->device(),
        VK_SHADER_STAGE_FRAGMENT_BIT,
        "text.frag")};
    assert(fragment_shader);
    [[maybe_unused]] boost::scope::defer_guard const destroy_frag{
        [this, shd = &fragment_shader.value()]()
        { destroy(&backend_->device(), shd); }};

    auto descriptor_layout{
        vkglsl::descriptor_set_layout(shader_set, backend_->device(), 0)};
    assert(descriptor_layout);
    text_descriptor_layout_ = *descriptor_layout;

    vkrndr::create_descriptor_sets(backend_->device(),
        backend_->descriptor_pool(),
        cppext::as_span(text_descriptor_layout_),
        cppext::as_span(text_descriptor_));

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

    text_pipeline_ =
        vkrndr::pipeline_builder_t{backend_->device(),
            vkrndr::pipeline_layout_builder_t{backend_->device()}
                .add_descriptor_set_layout(*descriptor_layout)
                .add_push_constants<glm::mat4>(VK_SHADER_STAGE_VERTEX_BIT)
                .build()}
            .add_shader(as_pipeline_shader(*vertex_shader))
            .add_shader(as_pipeline_shader(*fragment_shader))
            .add_color_attachment(backend_->image_format(), alpha_blend)
            .add_vertex_input(binding_description(), attribute_description())
            .build();

    for (auto& data : cppext::as_span(frame_data_))
    {
        data.vertex_buffer = vkrndr::create_buffer(backend_->device(),
            {.size = 4000 * sizeof(vertex_t),
                .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                .allocation_flags =
                    VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
                .required_memory_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT});

        data.vertex_map =
            vkrndr::map_memory(backend_->device(), data.vertex_buffer);

        data.index_buffer = vkrndr::create_buffer(backend_->device(),
            {.size = 6000 * sizeof(uint32_t),
                .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                .allocation_flags =
                    VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
                .required_memory_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT});

        data.index_map =
            vkrndr::map_memory(backend_->device(), data.index_buffer);
    }

    projection_.set_invert_y(false);
    resize(backend_->extent().width, backend_->extent().height);
}

reshed::text_editor_t::~text_editor_t()
{
    for (auto& data : cppext::as_span(frame_data_))
    {
        vkrndr::unmap_memory(backend_->device(), &data.index_map);

        vkrndr::destroy(&backend_->device(), &data.index_buffer);

        vkrndr::unmap_memory(backend_->device(), &data.vertex_map);

        vkrndr::destroy(&backend_->device(), &data.vertex_buffer);
    }

    destroy(&backend_->device(), &text_pipeline_);

    vkrndr::free_descriptor_sets(backend_->device(),
        backend_->descriptor_pool(),
        cppext::as_span(text_descriptor_));

    vkDestroyDescriptorSetLayout(backend_->device().logical,
        text_descriptor_layout_,
        nullptr);

    vkDestroySampler(backend_->device().logical, bitmap_sampler_, nullptr);

    destroy(&backend_->device(), &font_bitmap_);
}

void reshed::text_editor_t::handle_event(SDL_Event const& event)
{
    assert(event.type == SDL_EVENT_TEXT_INPUT);
    SDL_TextInputEvent const& text_event{event.text};
    buffer_ += text_event.text;
}

void reshed::text_editor_t::change_font(ngntxt::font_face_ptr_t font_face)
{
    font_face_ = std::move(font_face);

    destroy(&backend_->device(), &font_bitmap_);
    font_bitmap_ = ngntxt::create_bitmap(*backend_,
        *font_face_,
        0,
        256,
        ngntxt::font_bitmap_indexing_t::glyph);

    shaping_font_face_ = ngntxt::create_shaping_font_face(font_face_);

    update_descriptor_set(backend_->device(),
        text_descriptor_,
        vkrndr::combined_sampler_descriptor(bitmap_sampler_,
            font_bitmap_.bitmap));
}

VkPipelineLayout reshed::text_editor_t::pipeline_layout() const
{
    return text_pipeline_.pipeline ? *text_pipeline_.layout : VK_NULL_HANDLE;
}

void reshed::text_editor_t::draw(VkCommandBuffer command_buffer)
{
    {
        hb_buffer_clear_contents(shaping_buffer_.get());
        hb_buffer_add_utf8(shaping_buffer_.get(), buffer_.c_str(), -1, 0, -1);
        hb_buffer_guess_segment_properties(shaping_buffer_.get());

        hb_shape(shaping_font_face_.get(), shaping_buffer_.get(), nullptr, 0);

        unsigned int const len{hb_buffer_get_length(shaping_buffer_.get())};
        hb_glyph_info_t const* const info{
            hb_buffer_get_glyph_infos(shaping_buffer_.get(), nullptr)};
        hb_glyph_position_t const* const pos{
            hb_buffer_get_glyph_positions(shaping_buffer_.get(), nullptr)};

        vertex_t* const vertices{frame_data_->vertex_map.as<vertex_t>()};
        uint32_t* const indices{frame_data_->index_map.as<uint32_t>()};

        glm::ivec2 cursor{0, (*font_face_)->size->metrics.height >> 6};
        for (unsigned int i{}; i != len; i++)
        {
            ngntxt::glyph_info_t const& bitmap_glyph{
                font_bitmap_.glyphs.at(info[i].codepoint)};

            auto const& top_left{bitmap_glyph.top_left};
            auto const& size{bitmap_glyph.size};
            glm::vec2 const b{cppext::as_fp(bitmap_glyph.bearing.x),
                cppext::as_fp(bitmap_glyph.size.y - bitmap_glyph.bearing.y)};

            float const x_offset{cppext::as_fp(pos[i].x_offset >> 6)};
            float const y_offset{cppext::as_fp(pos[i].y_offset >> 6)};

            uint32_t const vert_idx{frame_data_->vertices};
            vertices[vert_idx + 0] = {glm::vec2{cursor.x, cursor.y},
                glm::vec2{top_left.x, top_left.y + size.y}};
            vertices[vert_idx + 1] = {
                glm::vec2{cursor.x + size.x + x_offset, cursor.y},
                glm::vec2{top_left.x + size.x, top_left.y + size.y}};
            vertices[vert_idx + 2] = {glm::vec2{cursor.x + size.x + x_offset,
                                          cursor.y - size.y - y_offset},
                glm::vec2{top_left.x + size.x, top_left.y}};
            vertices[vert_idx + 3] = {
                glm::vec2{cursor.x, cursor.y - size.y - y_offset},
                glm::vec2{top_left.x, top_left.y}};

            for (uint32_t j{vert_idx}; j != vert_idx + 4; ++j)
            {
                vertices[j].position += b;
            }

            uint32_t const index_idx{frame_data_->indices};
            indices[index_idx + 0] = vert_idx + 0;
            indices[index_idx + 1] = vert_idx + 1;
            indices[index_idx + 2] = vert_idx + 2;
            indices[index_idx + 3] = vert_idx + 2;
            indices[index_idx + 4] = vert_idx + 3;
            indices[index_idx + 5] = vert_idx + 0;

            frame_data_->indices += 6;
            frame_data_->vertices += 4;

            cursor += glm::ivec2{pos[i].x_advance, pos[i].y_advance} >> 6;
        }
    }

    bind_pipeline(command_buffer,
        text_pipeline_,
        0,
        cppext::as_span(text_descriptor_));

    VkDeviceSize const zero_offset{};
    vkCmdBindVertexBuffers(command_buffer,
        0,
        1,
        &frame_data_->vertex_buffer.buffer,
        &zero_offset);

    vkCmdBindIndexBuffer(command_buffer,
        frame_data_->index_buffer.buffer,
        0,
        VK_INDEX_TYPE_UINT32);

    vkCmdPushConstants(command_buffer,
        pipeline_layout(),
        VK_SHADER_STAGE_VERTEX_BIT,
        0,
        sizeof(glm::mat4),
        &projection_.projection_matrix());

    vkCmdDrawIndexed(command_buffer, frame_data_->indices, 1, 0, 0, 0);

    frame_data_.cycle(
        []([[maybe_unused]] auto const& prev, auto& next)
        {
            next.vertices = 0;
            next.indices = 0;
        });
}

void reshed::text_editor_t::resize(uint32_t const width, uint32_t const height)
{
    projection_.set_left_right({0.0f, cppext::as_fp(width)});
    projection_.set_bottom_top({cppext::as_fp(height), 0.0f});
    projection_.set_near_far_planes({-1.0f, 1.0f});

    projection_.update(glm::mat4{});
}
