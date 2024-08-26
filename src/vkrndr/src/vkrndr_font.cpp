#include <vkrndr_font.hpp>

#include <vkrndr_image.hpp>

void vkrndr::destroy(device_t const* device, font_t* const font)
{
    if (font)
    {
        font->character_bitmaps.clear();
        destroy(device, &font->texture);
    }
}
