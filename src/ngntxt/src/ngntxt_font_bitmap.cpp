#include <ngntxt_font_bitmap.hpp>

#include <ngntxt_font_face.hpp>

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
#include <ranges>
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

    void create_atlas_for_codepoints(vkrndr::backend_t& backend,
        ngntxt::font_bitmap_t& bitmap,
        std::ranges::forward_range auto&& codepoints)
    {
        std::map<char32_t, ngntxt::glyph_info_t> new_glpyhs;

        // Calculate required memory for tightly packing all bitmaps into a
        // staging buffer
        size_t all_bitmaps_size{};
        glm::uvec2 max_glyph_extents{};
        for (char32_t codepoint : codepoints)
        {
            FT_UInt const index{
                bitmap.indexing == ngntxt::font_bitmap_indexing_t::glyph
                    ? codepoint
                    : FT_Get_Char_Index(bitmap.font_face.get(), codepoint)};
            if (bitmap.indexing == ngntxt::font_bitmap_indexing_t::glyph &&
                new_glpyhs.contains(index))
            {
                continue;
            }

            if (FT_Error const error{FT_Load_Glyph(bitmap.font_face.get(),
                    index,
                    FT_LOAD_DEFAULT)})
            {
                spdlog::error("Glyph for codepoint {} not loaded. Error = {}",
                    static_cast<uint32_t>(codepoint),
                    error);
                continue;
            }

            FT_GlyphSlot const slot{bitmap.font_face->glyph};

            auto const& [it, inserted] =
                new_glpyhs.emplace(std::piecewise_construct,
                    std::forward_as_tuple(
                        bitmap.indexing == ngntxt::font_bitmap_indexing_t::glyph
                            ? index
                            : codepoint),
                    std::forward_as_tuple(glm::uvec2{},
                        glm::uvec2{slot->bitmap.width, slot->bitmap.rows},
                        glm::ivec2{slot->bitmap_left, slot->bitmap_top},
                        cppext::narrow<uint32_t>(slot->advance.x),
                        bitmap.bitmap_images.size()));
            auto const& glyph_size{it->second.size};

            [[maybe_unused]] bool const overflow{cppext::add(all_bitmaps_size,
                size_t{glyph_size.x} * glyph_size.y,
                all_bitmaps_size)};
            assert(overflow);

            max_glyph_extents = glm::max(max_glyph_extents, glyph_size);
        }

        size_t const glyph_count{new_glpyhs.size()};

        // Approximate a square size
        auto const horizontal_glyphs{
            std::max(static_cast<uint32_t>(
                         std::ceil(std::sqrtf(cppext::as_fp(glyph_count)))),
                uint32_t{1})};
        auto const vertical_glyphs{cppext::narrow<uint32_t>(
            (glyph_count + horizontal_glyphs - 1) / horizontal_glyphs)};

        // Prepare staging buffer and the target bitmap image
        vkrndr::buffer_t const staging_buffer{
            vkrndr::create_staging_buffer(backend.device(), all_bitmaps_size)};
        boost::scope::defer_guard const rollback{[&backend, staging_buffer]()
            { destroy(backend.device(), staging_buffer); }};
        vkrndr::mapped_memory_t staging_map{
            vkrndr::map_memory(backend.device(), staging_buffer)};

        vkrndr::image_t new_bitmap_image = vkrndr::create_image_and_view(
            backend.device(),
            vkrndr::image_2d_create_info_t{.format = VK_FORMAT_R8_UNORM,
                .extent = {(horizontal_glyphs + 1) * max_glyph_extents.x,
                    (vertical_glyphs + 1) * max_glyph_extents.y},
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                    VK_IMAGE_USAGE_SAMPLED_BIT,
                .required_memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
            VK_IMAGE_ASPECT_COLOR_BIT);
        boost::scope::scope_fail const rollback_bitmap{
            [&backend, &new_bitmap_image]()
            { destroy(backend.device(), new_bitmap_image); }};

        std::vector<VkBufferImageCopy> regions;
        regions.reserve(glyph_count);

        // Pack bitmaps into the staging buffer
        {
            std::byte* const staging_values{staging_map.as<std::byte>()};
            VkDeviceSize buffer_offset{};
            glm::uvec2 top_left{};
            for (auto& [value, glyph_info] : new_glpyhs)
            {
                constexpr auto const load_flags{FT_LOAD_RENDER};
                if (bitmap.indexing == ngntxt::font_bitmap_indexing_t::glyph)
                {
                    if (FT_Load_Glyph(bitmap.font_face.get(),
                            value,
                            load_flags))
                    {
                        continue;
                    }
                }
                else
                {
                    if (FT_Load_Char(bitmap.font_face.get(), value, load_flags))
                    {
                        continue;
                    }
                }

                FT_GlyphSlot const slot{bitmap.font_face->glyph};

                if (top_left.x + slot->bitmap.width >=
                    new_bitmap_image.extent.width)
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

                    regions.push_back(
                        VkBufferImageCopy{.bufferOffset = buffer_offset,
                            .bufferRowLength = slot->bitmap.width,
                            .bufferImageHeight = slot->bitmap.rows,
                            .imageSubresource = bitmap_subresource,
                            .imageOffset = {cppext::narrow<int>(top_left.x),
                                cppext::narrow<int>(top_left.y),
                                0},
                            .imageExtent = {slot->bitmap.width,
                                slot->bitmap.rows,
                                1}});

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
            vkrndr::wait_for_transfer_write(new_bitmap_image, cb, 1);

            vkCmdCopyBufferToImage(cb,
                staging_buffer,
                new_bitmap_image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                vkrndr::count_cast(regions),
                regions.data());

            vkrndr::wait_for_transfer_write_completed(new_bitmap_image, cb, 1);
        }

        bitmap.bitmap_images.push_back(new_bitmap_image);
        bitmap.glyphs.merge(std::move(new_glpyhs));
    }
} // namespace

ngntxt::font_bitmap_t ngntxt::create_bitmap(font_face_ptr_t font_face,
    font_bitmap_indexing_t const indexing)
{
    return {.indexing = indexing, .font_face = std::move(font_face)};
}

size_t ngntxt::load_codepoint_range(vkrndr::backend_t& backend,
    font_bitmap_t& bitmap,
    char32_t const begin,
    char32_t const end)
{
    create_atlas_for_codepoints(backend, bitmap, std::views::iota(begin, end));
    return bitmap.bitmap_images.size() - 1;
}

size_t ngntxt::load_codepoints(vkrndr::backend_t& backend,
    font_bitmap_t& bitmap,
    std::span<char32_t const> const& codepoints)
{
    create_atlas_for_codepoints(backend, bitmap, codepoints);
    return bitmap.bitmap_images.size() - 1;
}

void ngntxt::destroy(vkrndr::device_t const& device,
    font_bitmap_t const& bitmap)
{
    for (vkrndr::image_t const& bitmap_image : bitmap.bitmap_images)
    {
        destroy(device, bitmap_image);
    }
}
