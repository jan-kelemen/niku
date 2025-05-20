#ifndef NGNTXT_FONT_BITMAP_INCLUDED
#define NGNTXT_FONT_BITMAP_INCLUDED

#include <vkrndr_image.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H // IWYU pragma: keep

#include <glm/vec2.hpp>

#include <cstdint>
#include <map>

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
    };

    struct [[nodiscard]] font_bitmap_t final
    {
        std::map<char32_t, glyph_info_t> glyphs;
        vkrndr::image_t bitmap;
    };

    enum class [[nodiscard]] font_bitmap_indexing_t
    {
        codepoint,
        glyph
    };

    font_bitmap_t create_bitmap(vkrndr::backend_t& backend,
        FT_Face font_face,
        char32_t first_codepoint,
        char32_t last_codepoint,
        font_bitmap_indexing_t indexing);

    void destroy(vkrndr::device_t const* device, font_bitmap_t* bitmap);
} // namespace ngntxt

#endif
