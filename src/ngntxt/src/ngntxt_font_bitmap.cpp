#include <ngntxt_font_bitmap.hpp>

#include <cppext_numeric.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_buffer.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_memory.hpp>

#include <boost/scope/defer.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H // IWYU pragma: keep

#include <glm/common.hpp>

#include <spdlog/spdlog.h>

#include <volk.h>

#include <cassert>
#include <cmath>
#include <cstddef>
#include <span>
#include <tuple>
#include <utility>

// IWYU pragma: no_include <fmt/base.h>
// IWYU pragma: no_include <fmt/format.h>

ngntxt::font_bitmap_t ngntxt::create_bitmap(vkrndr::backend_t& backend,
    FT_Face font_face,
    char32_t const first_codepoint,
    char32_t const last_codepoint)
{
    assert(first_codepoint < last_codepoint);

    font_bitmap_t rv;

    uint32_t glyph_count{};
    glm::uvec2 max_glyph_extents{};
    for (char32_t codepoint{first_codepoint}; codepoint != last_codepoint;
        ++codepoint)
    {
        if (FT_Error const error{
                FT_Load_Char(font_face, codepoint, FT_LOAD_DEFAULT)})
        {
            spdlog::error("Character with codepoint {} not loaded. Error = {}",
                static_cast<uint32_t>(codepoint),
                error);
            continue;
        }

        FT_GlyphSlot const slot{font_face->glyph};
        ++glyph_count;

        rv.glyphs.emplace(std::piecewise_construct,
            std::forward_as_tuple(codepoint),
            std::forward_as_tuple(
                glm::ivec2{slot->bitmap.width, slot->bitmap.rows},
                glm::ivec2{slot->bitmap_left, slot->bitmap_top},
                cppext::narrow<uint32_t>(slot->advance.x)));

        max_glyph_extents = glm::max(max_glyph_extents,
            glm::uvec2{slot->bitmap.width, slot->bitmap.rows});
    }
    assert(glyph_count);

    // Approximate a square size
    auto const vertical_glyphs{static_cast<uint32_t>(
        glm::max(std::sqrtf(cppext::as_fp(glyph_count)), 1.0f))};
    auto const horizontal_glyphs{static_cast<uint32_t>(std::ceil(
        cppext::as_fp(glyph_count) / cppext::as_fp(vertical_glyphs)))};

    vkrndr::buffer_t staging_buffer{
        vkrndr::create_staging_buffer(backend.device(),
            VkDeviceSize{vertical_glyphs} *
                horizontal_glyphs *
                max_glyph_extents.x *
                max_glyph_extents.y)};
    boost::scope::defer_guard const rollback{
        [&backend, staging_buffer]() mutable
        { destroy(&backend.device(), &staging_buffer); }};

    vkrndr::mapped_memory_t staging_map{
        vkrndr::map_memory(backend.device(), staging_buffer)};

    std::span const staging_block{staging_map.as<std::byte>(),
        staging_buffer.size};
    auto const staging_block_pitch{horizontal_glyphs * max_glyph_extents.x};

    glyph_count = 0;

    for (char32_t codepoint{first_codepoint}; codepoint != last_codepoint;
        ++codepoint)
    {
        if (FT_Load_Char(font_face, codepoint, FT_LOAD_RENDER))
        {
            continue;
        }

        FT_GlyphSlot const slot{font_face->glyph};

        auto const pitch{cppext::narrow<unsigned int>(slot->bitmap.pitch)};
        auto const horizontal_glyph_start{
            (glyph_count % horizontal_glyphs) * max_glyph_extents.x};
        auto const vertical_glyph_start{
            (glyph_count / vertical_glyphs) * max_glyph_extents.y};

        for (unsigned int y{}; y != slot->bitmap.rows; ++y)
        {
            for (unsigned int x{}; x != slot->bitmap.width; ++x)
            {
                staging_block[(vertical_glyph_start + y) * staging_block_pitch +
                    horizontal_glyph_start +
                    x] =
                    static_cast<std::byte>(slot->bitmap.buffer[y * pitch + x]);
            }
        }

        ++glyph_count;
    }

    unmap_memory(backend.device(), &staging_map);

    rv.bitmap = backend.transfer_buffer_to_image(staging_buffer,
        {horizontal_glyphs * max_glyph_extents.x,
            vertical_glyphs * max_glyph_extents.y},
        VK_FORMAT_R8_UNORM,
        1);

    return rv;
}

void ngntxt::destroy(vkrndr::device_t const* device, font_bitmap_t* bitmap)
{
    if (bitmap)
    {
        destroy(device, &bitmap->bitmap);
    }
}
