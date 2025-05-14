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
    char32_t const last_codepoint,
    font_bitmap_indexing_t const indexing)
{
    assert(first_codepoint < last_codepoint);

    font_bitmap_t rv;

    glm::uvec2 max_glyph_extents{};
    for (char32_t codepoint{first_codepoint}; codepoint != last_codepoint;
        ++codepoint)
    {
        FT_UInt const index{FT_Get_Char_Index(font_face, codepoint)};
        if (indexing == font_bitmap_indexing_t::glyph &&
            rv.glyphs.contains(index))
        {
            continue;
        }

        if (FT_Error const error{
                FT_Load_Glyph(font_face, index, FT_LOAD_DEFAULT)})
        {
            spdlog::error("Glyph for codepoint {} not loaded. Error = {}",
                static_cast<uint32_t>(codepoint),
                error);
            continue;
        }

        FT_GlyphSlot const slot{font_face->glyph};

        rv.glyphs.emplace(std::piecewise_construct,
            std::forward_as_tuple(
                indexing == font_bitmap_indexing_t::glyph ? index : codepoint),
            std::forward_as_tuple(
                glm::ivec2{slot->bitmap.width, slot->bitmap.rows},
                glm::ivec2{slot->bitmap_left, slot->bitmap_top},
                cppext::narrow<uint32_t>(slot->advance.x)));

        max_glyph_extents = glm::max(max_glyph_extents,
            glm::uvec2{slot->bitmap.width, slot->bitmap.rows});
    }

    size_t const glyph_count{rv.glyphs.size()};

    // Approximate a square size
    auto const horizontal_glyphs{
        std::max(static_cast<uint32_t>(
                     std::ceil(std::sqrtf(cppext::as_fp(glyph_count)))),
            uint32_t{1})};
    auto const vertical_glyphs{cppext::narrow<uint32_t>(
        (glyph_count + horizontal_glyphs - 1) / horizontal_glyphs)};

    vkrndr::buffer_t staging_buffer{
        vkrndr::create_staging_buffer(backend.device(),
            (horizontal_glyphs + 1) *
                max_glyph_extents.x *
                (vertical_glyphs + 1) *
                max_glyph_extents.y)};
    boost::scope::defer_guard const rollback{
        [&backend, staging_buffer]() mutable
        { destroy(&backend.device(), &staging_buffer); }};

    vkrndr::mapped_memory_t staging_map{
        vkrndr::map_memory(backend.device(), staging_buffer)};

    std::span const staging_block{staging_map.as<std::byte>(),
        staging_buffer.size};
    auto const staging_block_pitch{horizontal_glyphs * max_glyph_extents.x};

    glm::uvec2 top_left{};
    for (auto& [value, rect] : rv.glyphs)
    {
        auto const load_flags{FT_LOAD_RENDER};
        if (indexing == font_bitmap_indexing_t::glyph)
        {
            if (FT_Load_Glyph(font_face, value, load_flags))
            {
                continue;
            }
        }
        else
        {
            if (FT_Load_Char(font_face, value, load_flags))
            {
                continue;
            }
        }

        FT_GlyphSlot const slot{font_face->glyph};

        if (top_left.x + slot->bitmap.width >= staging_block_pitch)
        {
            top_left.x = 0;
            top_left.y += staging_block_pitch * max_glyph_extents.y;
        }

        auto const bitmap_pitch{
            cppext::narrow<unsigned int>(slot->bitmap.pitch)};
        for (unsigned int y{}; y != slot->bitmap.rows; ++y)
        {
            for (unsigned int x{}; x != slot->bitmap.width; ++x)
            {
                staging_block
                    [top_left.y + y * staging_block_pitch + top_left.x + x] =
                        static_cast<std::byte>(
                            slot->bitmap.buffer[y * bitmap_pitch + x]);
            }
        }

        rect.top_left = {top_left.x, top_left.y / staging_block_pitch};

        top_left.x += slot->bitmap.rows;
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
