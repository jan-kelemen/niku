#ifndef VKRNDR_FONT_MANAGER_INCLUDED
#define VKRNDR_FONT_MANAGER_INCLUDED

#include <vulkan_font.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H // IWYU pragma: keep

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <unordered_map>
#include <vector>

namespace vkrndr
{
    struct [[nodiscard]] font_bitmap final
    {
        uint32_t bitmap_width{};
        uint32_t bitmap_height{};
        std::unordered_map<char, character_bitmap> bitmaps;
        std::vector<std::byte> bitmap_data;
    };

    class [[nodiscard]] font_manager final
    {
    public: // Construction
        font_manager();

        font_manager(font_manager const&) = delete;

        font_manager(font_manager&&) noexcept = delete;

    public: // Destruction
        ~font_manager();

    public: // Interface
        font_bitmap load_font_bitmap(std::filesystem::path const& font_file,
            uint32_t font_size);

    public: // Operators
        font_manager& operator=(font_manager const&) = delete;

        font_manager& operator=(font_manager&&) noexcept = delete;

    private: // Data
        FT_Library library_handle_{nullptr};
    };
} // namespace vkrndr

#endif // !VKRNDR_FONT_MANGER_INCLUDED
