#ifndef VKRNDR_FONT_INCLUDED
#define VKRNDR_FONT_INCLUDED

#include <vkrndr_image.hpp>

#include <glm/vec2.hpp>

#include <cstdint>
#include <unordered_map>

// IWYU pragma: no_include <glm/detail/qualifier.hpp>

namespace vkrndr
{
    struct device_t;
} // namespace vkrndr

namespace vkrndr
{
    struct [[nodiscard]] character_bitmap_t final
    {
        glm::ivec2 size;
        glm::ivec2 bearing;
        uint32_t offset;
        uint32_t advance;
    };

    struct [[nodiscard]] font_t final
    {
        std::unordered_map<char, character_bitmap_t> character_bitmaps;
        uint32_t bitmap_width{};
        uint32_t bitmap_height{};
        image_t texture;
    };

    void destroy(device_t const* device, font_t* font);
} // namespace vkrndr

#endif
