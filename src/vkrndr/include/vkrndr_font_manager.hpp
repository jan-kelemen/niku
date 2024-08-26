#ifndef VKRNDR_FONT_MANAGER_INCLUDED
#define VKRNDR_FONT_MANAGER_INCLUDED

#include <vkrndr_font.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H // IWYU pragma: keep

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <unordered_map>
#include <vector>

namespace vkrndr
{
    struct [[nodiscard]] font_bitmap_t final
    {
        uint32_t bitmap_width{};
        uint32_t bitmap_height{};
        std::unordered_map<char, character_bitmap_t> bitmaps;
        std::vector<std::byte> bitmap_data;
    };

    class [[nodiscard]] font_manager_t final
    {
    public: // Construction
        font_manager_t();

        font_manager_t(font_manager_t const&) = delete;

        font_manager_t(font_manager_t&&) noexcept = delete;

    public: // Destruction
        ~font_manager_t();

    public: // Interface
        font_bitmap_t load_font_bitmap(std::filesystem::path const& font_file,
            uint32_t font_size);

    public: // Operators
        font_manager_t& operator=(font_manager_t const&) = delete;

        font_manager_t& operator=(font_manager_t&&) noexcept = delete;

    private: // Data
        FT_Library library_handle_{nullptr};
    };
} // namespace vkrndr

#endif // !VKRNDR_FONT_MANGER_INCLUDED
