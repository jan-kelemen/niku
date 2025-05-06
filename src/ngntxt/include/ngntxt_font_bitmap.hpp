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
} // namespace vkrndr

namespace ngntxt
{
    struct [[nodiscard]] glyph_info_t final
    {
        glm::ivec2 size;
        glm::ivec2 bearing;
        uint32_t advance;
    };

    struct [[nodiscard]] font_bitmap_t final
    {
        std::map<char32_t, glyph_info_t> glyphs;
        vkrndr::image_t bitmap;
    };

    font_bitmap_t create_bitmap(vkrndr::backend_t& backend,
        FT_Face font_face,
        char32_t first_codepoint,
        char32_t last_codepoint);
} // namespace ngntxt

#endif
