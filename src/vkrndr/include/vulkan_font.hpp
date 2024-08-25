#ifndef VKRNDR_VULKAN_FONT_INCLUDED
#define VKRNDR_VULKAN_FONT_INCLUDED

#include <vulkan_image.hpp>

#include <glm/vec2.hpp>

#include <cstdint>
#include <unordered_map>

// IWYU pragma: no_include <glm/detail/qualifier.hpp>

namespace vkrndr
{
    struct vulkan_device;
} // namespace vkrndr

namespace vkrndr
{
    struct [[nodiscard]] character_bitmap final
    {
        glm::ivec2 size;
        glm::ivec2 bearing;
        uint32_t offset;
        uint32_t advance;
    };

    struct [[nodiscard]] vulkan_font final
    {
        std::unordered_map<char, character_bitmap> character_bitmaps;
        uint32_t bitmap_width{};
        uint32_t bitmap_height{};
        vulkan_image texture;
    };

    void destroy(vulkan_device const* device, vulkan_font* font);
} // namespace vkrndr

#endif // !VKRNDR_VULKAN_FONT_INCLUDED
