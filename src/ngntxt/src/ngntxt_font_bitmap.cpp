#include <ngntxt_font_bitmap.hpp>

#include <cppext_numeric.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_buffer.hpp>
#include <vkrndr_commands.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_memory.hpp>
#include <vkrndr_transient_operation.hpp>
#include <vkrndr_utility.hpp>

#include <boost/scope/defer.hpp>
#include <boost/scope/scope_fail.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H // IWYU pragma: keep

#include <glm/common.hpp>

#include <spdlog/spdlog.h>

#include <volk.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

// IWYU pragma: no_include <boost/scope/exception_checker.hpp>
// IWYU pragma: no_include <fmt/base.h>
// IWYU pragma: no_include <fmt/format.h>

namespace
{
    constexpr VkImageSubresourceLayers bitmap_subresource{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1};
} // namespace

ngntxt::font_bitmap_t ngntxt::create_bitmap(vkrndr::backend_t& backend,
    FT_Face font_face,
    char32_t const first_codepoint,
    char32_t const last_codepoint,
    font_bitmap_indexing_t const indexing)
{
    assert(first_codepoint < last_codepoint);

    font_bitmap_t rv;

    // Calculate required memory for tightly packing all bitmaps into a staging
    // buffer
    size_t all_bitmaps_size{};
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

        auto const& [it, inserted] = rv.glyphs.emplace(std::piecewise_construct,
            std::forward_as_tuple(
                indexing == font_bitmap_indexing_t::glyph ? index : codepoint),
            std::forward_as_tuple(glm::uvec2{},
                glm::uvec2{slot->bitmap.width, slot->bitmap.rows},
                glm::ivec2{slot->bitmap_left, slot->bitmap_top},
                cppext::narrow<uint32_t>(slot->advance.x)));
        auto const& glyph_size{it->second.size};

        all_bitmaps_size += size_t{glyph_size.x} * glyph_size.y;

        max_glyph_extents = glm::max(max_glyph_extents, glyph_size);
    }

    size_t const glyph_count{rv.glyphs.size()};

    // Approximate a square size
    auto const horizontal_glyphs{
        std::max(static_cast<uint32_t>(
                     std::ceil(std::sqrtf(cppext::as_fp(glyph_count)))),
            uint32_t{1})};
    auto const vertical_glyphs{cppext::narrow<uint32_t>(
        (glyph_count + horizontal_glyphs - 1) / horizontal_glyphs)};

    // Prepare staging buffer and the target bitmap image
    vkrndr::buffer_t staging_buffer{
        vkrndr::create_staging_buffer(backend.device(), all_bitmaps_size)};
    boost::scope::defer_guard const rollback{
        [&backend, staging_buffer]() mutable
        { destroy(&backend.device(), &staging_buffer); }};
    vkrndr::mapped_memory_t staging_map{
        vkrndr::map_memory(backend.device(), staging_buffer)};

    rv.bitmap = vkrndr::create_image_and_view(backend.device(),
        vkrndr::image_2d_create_info_t{.format = VK_FORMAT_R8_UNORM,
            .extent = {(horizontal_glyphs + 1) * max_glyph_extents.x,
                (vertical_glyphs + 1) * max_glyph_extents.y},
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage =
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .required_memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
        VK_IMAGE_ASPECT_COLOR_BIT);
    boost::scope::scope_fail const rollback_bitmap{
        [&backend, b = &rv.bitmap]() { destroy(&backend.device(), b); }};

    std::vector<VkBufferImageCopy> regions;
    regions.reserve(glyph_count);

    // Pack bitmaps into the staging buffer
    {
        std::byte* const staging_values{staging_map.as<std::byte>()};
        VkDeviceSize buffer_offset{};
        glm::uvec2 top_left{};
        for (auto& [value, glyph_info] : rv.glyphs)
        {
            constexpr auto const load_flags{FT_LOAD_RENDER};
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

            if (top_left.x + slot->bitmap.width >= rv.bitmap.extent.width)
            {
                top_left.x = 0;
                top_left.y += max_glyph_extents.y;
            }

            if (auto const bitmap_pitch{
                    cppext::narrow<unsigned int>(slot->bitmap.pitch)})
            {
                for (unsigned int y{}; y != slot->bitmap.rows; ++y)
                {
                    for (unsigned int x{}; x != slot->bitmap.width; ++x)
                    {
                        staging_values[buffer_offset +
                            VkDeviceSize{y} * slot->bitmap.width +
                            x] =
                            static_cast<std::byte>(
                                slot->bitmap.buffer[y * bitmap_pitch + x]);
                    }
                }

                regions.push_back(VkBufferImageCopy{
                    .bufferOffset = buffer_offset,
                    .bufferRowLength = slot->bitmap.width,
                    .bufferImageHeight = slot->bitmap.rows,
                    .imageSubresource = bitmap_subresource,
                    .imageOffset = {cppext::narrow<int>(top_left.x),
                        cppext::narrow<int>(top_left.y),
                        0},
                    .imageExtent = {slot->bitmap.width, slot->bitmap.rows, 1}});

                buffer_offset +=
                    VkDeviceSize{slot->bitmap.width} * slot->bitmap.rows;
            }

            glyph_info.top_left = top_left;
            top_left.x += slot->bitmap.width;
        }

        unmap_memory(backend.device(), &staging_map);
    }

    // Transfer staging buffer to the target bitmap
    {
        auto transient{backend.request_transient_operation(true)};
        VkCommandBuffer cb{transient.command_buffer()};
        vkrndr::wait_for_transfer_write(rv.bitmap.image, cb, 1);

        vkCmdCopyBufferToImage(cb,
            staging_buffer.buffer,
            rv.bitmap.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            vkrndr::count_cast(regions.size()),
            regions.data());

        vkrndr::wait_for_transfer_write_completed(rv.bitmap.image, cb, 1);
    }

    return rv;
}

void ngntxt::destroy(vkrndr::device_t const* device, font_bitmap_t* bitmap)
{
    if (bitmap)
    {
        destroy(device, &bitmap->bitmap);
    }
}
