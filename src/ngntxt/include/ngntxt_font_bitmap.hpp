#ifndef NGNTXT_FONT_BITMAP_INCLUDED
#define NGNTXT_FONT_BITMAP_INCLUDED

#include <vkrndr_image.hpp>

#include <ngntxt_font_face.hpp>

#include <glm/vec2.hpp>

#include <cstdint>
#include <map>
#include <span>
#include <vector>

namespace vkrndr
{
    class backend_t;
    struct device_t;
} // namespace vkrndr

namespace ngntxt
{
    struct [[nodiscard]] glyph_info_t final
    {
        glm::uvec2 top_left;
        glm::uvec2 size;
        glm::ivec2 bearing;
        uint32_t advance;
        size_t bitmap_image_index;
    };

    enum class [[nodiscard]] font_bitmap_indexing_t
    {
        codepoint,
        glyph
    };

    struct [[nodiscard]] font_bitmap_t final
    {
        std::map<char32_t, glyph_info_t> glyphs;
        std::vector<vkrndr::image_t> bitmap_images;
        font_bitmap_indexing_t indexing{font_bitmap_indexing_t::glyph};
        font_face_ptr_t font_face;
    };

    font_bitmap_t create_bitmap(font_face_ptr_t font_face,
        font_bitmap_indexing_t indexing);

    [[nodiscard]] size_t load_codepoint_range(vkrndr::backend_t& backend,
        font_bitmap_t& bitmap,
        char32_t begin,
        char32_t end);

    [[nodiscard]] size_t load_codepoints(vkrndr::backend_t& backend,
        font_bitmap_t& bitmap,
        std::span<char32_t const> const& codepoints);

    void destroy(vkrndr::device_t const* device, font_bitmap_t* bitmap);
} // namespace ngntxt

#endif
