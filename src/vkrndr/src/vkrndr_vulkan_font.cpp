#include <vkrndr_vulkan_font.hpp>

#include <vkrndr_vulkan_image.hpp>

void vkrndr::destroy(vulkan_device const* device, vulkan_font* const font)
{
    if (font)
    {
        font->character_bitmaps.clear();
        destroy(device, &font->texture);
    }
}
