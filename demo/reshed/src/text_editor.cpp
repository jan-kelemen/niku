#include <text_editor.hpp>

#include <config.hpp>
#include <text_buffer.hpp>

#include <cppext_container.hpp>
#include <cppext_cycled_buffer.hpp>
#include <cppext_numeric.hpp>

#include <ngngfx_orthographic_projection.hpp>

#include <ngntxt_font_bitmap.hpp>
#include <ngntxt_font_face.hpp>
#include <ngntxt_shaping.hpp>
#include <ngntxt_syntax.hpp>

#include <vkglsl_shader_set.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_buffer.hpp>
#include <vkrndr_descriptors.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_memory.hpp>
#include <vkrndr_pipeline.hpp>
#include <vkrndr_shader_module.hpp>
#include <vkrndr_utility.hpp>

#include <boost/scope/defer.hpp>

#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H // IWYU pragma: keep

#include <hb.h>

#include <imgui.h>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_scancode.h>

#include <tree_sitter/api.h>

#include <tree-sitter-glsl.h>

#include <vma_impl.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <random>
#include <stdexcept>
#include <utility>
#include <string_view>

// IWYU pragma: no_include <fmt/base.h>
// IWYU pragma: no_include <fmt/format.h>
// IWYU pragma: no_include <expected>
// IWYU pragma: no_include <filesystem>
// IWYU pragma: no_include <functional>
// IWYU pragma: no_include <memory>
// IWYU pragma: no_include <system_error>
// IWYU pragma: no_include <span>

namespace
{
    struct [[nodiscard]] vertex_t final
    {
        glm::vec2 position;
        glm::vec2 size;
        glm::vec2 uv;
        glm::vec4 color;
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
                .offset = offsetof(vertex_t, size)},
            VkVertexInputAttributeDescription{.location = 2,
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(vertex_t, uv)},
            VkVertexInputAttributeDescription{.location = 3,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                .offset = offsetof(vertex_t, color)}};

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

    [[nodiscard]] std::string read_highlights(std::filesystem::path const& file)
    {
        std::ifstream stream{file, std::ios::ate | std::ios::binary};

        if (!stream.is_open())
        {
            throw std::runtime_error{"failed to open file!"};
        }

        auto const eof{stream.tellg()};

        std::string buffer;
        buffer.resize(static_cast<size_t>(eof));

        stream.seekg(0);

        stream.read(buffer.data(), eof);

        return buffer;
    }

    ngntxt::query_handle_t create_highlight_query(
        ngntxt::language_handle_t const& language)
    {
        std::string const queries{read_highlights("highlights.scm")};
        return ngntxt::create_query(language, queries);
    }

    struct [[nodiscard]] shaped_line_t final
    {
        std::span<vertex_t> vertices;
        std::vector<hb_glyph_info_t> glyph_infos;
    };
} // namespace

reshed::text_editor_t::text_editor_t(vkrndr::backend_t& backend)
    : backend_{&backend}
    , parser_{ngntxt::create_parser()}
    , language_{tree_sitter_glsl()}
    , highlight_query_{create_highlight_query(language_)}
    , shaping_buffer_{ngntxt::create_shaping_buffer()}
    , bitmap_sampler_{create_bitmap_sampler(backend_->device())}
    , frame_data_{backend_->frames_in_flight(), backend_->frames_in_flight()}
{
    [[maybe_unused]] bool const language_set{
        ngntxt::set_language(parser_, language_)};
    assert(language_set);

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

    auto tesselation_control_shader{add_shader_module_from_path(shader_set,
        backend_->device(),
        VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
        "text.tesc")};
    assert(tesselation_control_shader);
    [[maybe_unused]] boost::scope::defer_guard const destroy_tesc{
        [this, shd = &tesselation_control_shader.value()]()
        { destroy(&backend_->device(), shd); }};

    auto tesselation_evaluation_shader{add_shader_module_from_path(shader_set,
        backend_->device(),
        VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
        "text.tese")};
    assert(tesselation_evaluation_shader);
    [[maybe_unused]] boost::scope::defer_guard const destroy_tese{
        [this, shd = &tesselation_evaluation_shader.value()]()
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
                .add_push_constants<glm::mat4>(
                    VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
                .build()}
            .add_shader(as_pipeline_shader(*vertex_shader))
            .add_shader(as_pipeline_shader(*tesselation_control_shader))
            .add_shader(as_pipeline_shader(*tesselation_evaluation_shader))
            .add_shader(as_pipeline_shader(*fragment_shader))
            .add_color_attachment(backend_->image_format(), alpha_blend)
            .add_vertex_input(binding_description(), attribute_description())
            .with_primitive_topology(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST)
            .with_tesselation_patch_points(1)
            .build();

    for (auto& data : cppext::as_span(frame_data_))
    {
        data.vertex_buffer = vkrndr::create_buffer(backend_->device(),
            {.size = 40000 * sizeof(vertex_t),
                .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                .allocation_flags =
                    VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
                .required_memory_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT});

        data.vertex_map =
            vkrndr::map_memory(backend_->device(), data.vertex_buffer);
    }

    projection_.set_invert_y(false);
    resize(backend_->extent().width, backend_->extent().height);

    static_cast<void>(buffer_.add(0,
        0,
        "float Hammersley(uint i, uint N);\nvec2 Hammersley(uint i, uint N);"));

    tree_ = ngntxt::parse(parser_,
        tree_,
        [this]([[maybe_unused]] size_t byte,
            size_t row,
            size_t column) -> std::string_view
        { return buffer_.line(row, true).substr(column); });

    std::default_random_engine eng{std::random_device{}()};
    std::uniform_real_distribution dist{0.5f};

    std::ranges::transform(ngntxt::capture_names(highlight_query_),
        std::back_inserter(syntax_color_table_),
        [&eng, &dist](std::string_view name) -> syntax_color_entry_t
        {
            return {.name = std::string{name},
                .color = {dist(eng), dist(eng), dist(eng), 1.0f}};
        });
}

reshed::text_editor_t::~text_editor_t()
{
    for (auto& data : cppext::as_span(frame_data_))
    {
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
    if (event.type == SDL_EVENT_TEXT_INPUT)
    {
        SDL_TextInputEvent const& text_event{event.text};
        std::string_view const content{text_event.text};

        auto const& [start_byte, point] =
            buffer_.add(cursor_line, cursor_column, content);

        tree_ = ngntxt::edit(parser_,
            tree_,
            {.start = {.byte = start_byte,
                 .row = cursor_line,
                 .column = cursor_column},
                .old_end = {.byte = start_byte,
                    .row = cursor_line,
                    .column = cursor_column},
                .new_end = {.byte = point.byte,
                    .row = point.line,
                    .column = point.column}},
            [this]([[maybe_unused]] size_t byte,
                size_t row,
                size_t column) -> std::string_view
            { return buffer_.line(row, true).substr(column); });

        cursor_line = point.line;
        cursor_column = point.column;
    }
    else if (event.type == SDL_EVENT_KEY_DOWN)
    {
        SDL_KeyboardEvent const& keyboard_event{event.key};
        if (keyboard_event.scancode == SDL_SCANCODE_RETURN ||
            keyboard_event.scancode == SDL_SCANCODE_RETURN2 ||
            keyboard_event.scancode == SDL_SCANCODE_KP_ENTER)
        {
            auto const& [start_byte, point] =
                buffer_.add(cursor_line, cursor_column, "\n");

            tree_ = ngntxt::edit(parser_,
                tree_,
                {.start = {.byte = start_byte,
                     .row = cursor_line,
                     .column = cursor_column},
                    .old_end = {.byte = start_byte,
                        .row = cursor_line,
                        .column = cursor_column},
                    .new_end = {.byte = point.byte,
                        .row = point.line,
                        .column = point.column}},
                [this]([[maybe_unused]] size_t byte,
                    size_t row,
                    size_t column) -> std::string_view
                { return buffer_.line(row, true).substr(column); });

            cursor_line = point.line;
            cursor_column = point.column;
        }
        else if (keyboard_event.scancode == SDL_SCANCODE_BACKSPACE)
        {
            auto const& [start_byte, point] =
                buffer_.remove(cursor_line, cursor_column, 1);

            tree_ = ngntxt::edit(parser_,
                tree_,
                {.start = {.byte = start_byte,
                     .row = cursor_line,
                     .column = cursor_column},
                    .old_end = {.byte = start_byte,
                        .row = cursor_line,
                        .column = cursor_column},
                    .new_end = {.byte = point.byte,
                        .row = point.line,
                        .column = point.column}},
                [this]([[maybe_unused]] size_t byte,
                    size_t row,
                    size_t column) -> std::string_view
                { return buffer_.line(row, true).substr(column); });

            cursor_line = point.line;
            cursor_column = point.column;
        }
        else if (keyboard_event.scancode == SDL_SCANCODE_UP)
        {
            if (cursor_line > 0)
            {
                --cursor_line;
            }
        }
        else if (keyboard_event.scancode == SDL_SCANCODE_DOWN)
        {
            if (cursor_line < buffer_.lines())
            {
                ++cursor_line;
            }
        }
        else if (keyboard_event.scancode == SDL_SCANCODE_LEFT)
        {
            if (cursor_column > 0)
            {
                --cursor_column;
            }
        }
        else if (keyboard_event.scancode == SDL_SCANCODE_RIGHT)
        {
            ++cursor_column;
        }
    }
}

void reshed::text_editor_t::change_font(ngntxt::font_face_ptr_t font_face)
{
    font_face_ = std::move(font_face);

    destroy(&backend_->device(), &font_bitmap_);
    font_bitmap_ = ngntxt::create_bitmap(*backend_,
        font_face_.get(),
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
    ImGui::Begin("Syntax colors");
    for (syntax_color_entry_t& entry : syntax_color_table_)
    {
        ImGui::SliderFloat3(entry.name.data(),
            glm::value_ptr(entry.color),
            0.0f,
            1.0f);
    }
    ImGui::End();

    vertex_t* const vertices{frame_data_->vertex_map.as<vertex_t>()};

    std::vector<shaped_line_t> shaped_lines;

    float const line_height{
        cppext::as_fp(font_face_->size->metrics.height >> 6)};

    projection_.set_bottom_top(
        {cppext::as_fp(cursor_line) * line_height + cppext::as_fp(extent_.y),
            cppext::as_fp(cursor_line) * line_height});
    projection_.update(glm::mat4{});

    glm::vec2 cursor{0.0f, line_height};
    for (size_t line_index{}; line_index != buffer_.lines(); ++line_index)
    {
        std::string_view const line{buffer_.line(line_index, false)};

        hb_buffer_clear_contents(shaping_buffer_.get());

        // NOLINTBEGIN(bugprone-suspicious-stringview-data-usage)
        hb_buffer_add_utf8(shaping_buffer_.get(),
            line.data(),
            cppext::narrow<int>(line.size()),
            0,
            cppext::narrow<int>(line.size()));
        // NOLINTEND(bugprone-suspicious-stringview-data-usage)
        hb_buffer_guess_segment_properties(shaping_buffer_.get());

        hb_shape(shaping_font_face_.get(), shaping_buffer_.get(), nullptr, 0);

        unsigned int const len{hb_buffer_get_length(shaping_buffer_.get())};
        shaped_line_t& shaped_line{shaped_lines.emplace_back(
            std::span{vertices + frame_data_->vertices, len})};
        // cppcheck-suppress-begin invalidContainerReference
        std::ranges::copy_n(
            hb_buffer_get_glyph_infos(shaping_buffer_.get(), nullptr),
            len,
            std::back_inserter(shaped_line.glyph_infos));
        // cppcheck-suppress-end invalidContainerReference

        std::span<hb_glyph_position_t const> const positions{
            hb_buffer_get_glyph_positions(shaping_buffer_.get(), nullptr),
            len};

        for (unsigned int i{}; i != len; i++)
        {
            auto const glyph_it{
                font_bitmap_.glyphs.find(shaped_line.glyph_infos[i].codepoint)};
            ngntxt::glyph_info_t const& bitmap_glyph{
                glyph_it != font_bitmap_.glyphs.cend()
                    ? glyph_it->second
                    : font_bitmap_.glyphs.at(0)};

            auto const& top_left{bitmap_glyph.top_left};
            auto const& size{bitmap_glyph.size};

            glm::vec2 const bearing{cppext::as_fp(bitmap_glyph.bearing.x),
                cppext::as_fp(bitmap_glyph.size.y) -
                    cppext::as_fp(bitmap_glyph.bearing.y)};

            glm::vec2 const offset{cppext::as_fp(positions[i].x_offset >> 6),
                cppext::as_fp(positions[i].y_offset >> 6)};

            vertices[frame_data_->vertices++] = {cursor + offset + bearing,
                glm::vec2{size},
                glm::vec2{top_left},
                glm::vec4{1.0f, 1.0f, 1.0f, 1.0f}};

            cursor +=
                glm::ivec2{positions[i].x_advance, positions[i].y_advance} >> 6;
        }

        cursor.x = 0;
        cursor.y += line_height;
    }

    ngntxt::query_cursor_handle_t cursor_handle{
        ngntxt::execute_query(highlight_query_,
            ts_tree_root_node(tree_.get()))};
    while (std::optional<ngntxt::query_match_t> const match{
        ngntxt::next_match(cursor_handle)})
    {
        for (auto const& capture : match->captures)
        {
            TSPoint const start{ts_node_start_point(capture.node)};
            TSPoint const end{ts_node_end_point(capture.node)};
            assert(start.row == end.row);

            auto& row{shaped_lines[start.row]};
            for (size_t i{}; i != row.vertices.size() &&
                row.glyph_infos[i].cluster < end.column;
                ++i)
            {
                if (row.glyph_infos[i].cluster >= start.column)
                {
                    row.vertices[i].color =
                        syntax_color_table_[capture.index].color;
                }
            }
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

    vkCmdPushConstants(command_buffer,
        pipeline_layout(),
        VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
        0,
        sizeof(glm::mat4),
        &projection_.projection_matrix());

    vkCmdDraw(command_buffer, frame_data_->vertices, 1, 0, 0);

    frame_data_.cycle([]([[maybe_unused]] auto const& prev, auto& next)
        { next.vertices = 0; });
}

void reshed::text_editor_t::resize(uint32_t const width, uint32_t const height)
{
    extent_ = {width, height};

    projection_.set_left_right({0.0f, cppext::as_fp(width)});
    projection_.set_bottom_top({cppext::as_fp(height), 0.0f});
    projection_.set_near_far_planes({-1.0f, 1.0f});

    projection_.update(glm::mat4{});
}
