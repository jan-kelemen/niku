#include <vkrndr_font_manager.hpp>

#include <vkrndr_font.hpp>

#include <fmt/format.h>
#include <fmt/std.h> // IWYU pragma: keep

#include <glm/vec2.hpp>

#include <algorithm>
#include <limits>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <utility>

vkrndr::font_manager_t::font_manager_t()
{
    if (FT_Error const error{FT_Init_FreeType(&library_handle_)}; error)
    {
        throw std::runtime_error{
            fmt::format("Unable to initialize FreeType library. Error = {}",
                error)};
    }
}

vkrndr::font_manager_t::~font_manager_t() { FT_Done_FreeType(library_handle_); }

vkrndr::font_bitmap_t vkrndr::font_manager_t::load_font_bitmap(
    std::filesystem::path const& font_file,
    uint32_t const font_size)
{
    FT_Face font_face; // NOLINT
    if (FT_Error const error{FT_New_Face(library_handle_,
            font_file.string().c_str(),
            0,
            &font_face)};
        error)
    {
        throw std::runtime_error{
            fmt::format("Unable to load font {}. Error = {}",
                font_file,
                error)};
    }

    // TODO-JK: Proper RAII guard
    std::shared_ptr<FT_Face> font_face_guard{&font_face,
        [](FT_Face* font) { FT_Done_Face(*font); }};

    if (FT_Error const error{FT_Set_Pixel_Sizes(font_face, 0, font_size)};
        error)
    {
        throw std::runtime_error{
            fmt::format("Unable to set pixel size {}. Error = {}",
                font_size,
                error)};
    }

    font_bitmap_t rv;

    uint32_t bitmap_width{};
    unsigned int bitmap_height{std::numeric_limits<unsigned int>::min()};
    std::unordered_map<char, std::vector<std::byte>> individual_bitmaps;
    for (FT_ULong character_code{0}; character_code != 128; ++character_code)
    {
        if (FT_Error const error{
                FT_Load_Char(font_face, character_code, FT_LOAD_RENDER)})
        {
            throw std::runtime_error{
                fmt::format("Unable to load character code {}. Error = {}",
                    character_code,
                    error)};
        }

        auto const& current_glyph{*font_face->glyph};

        bitmap_height = std::max(bitmap_height, current_glyph.bitmap.rows);

        unsigned int const pitch{
            static_cast<unsigned int>(current_glyph.bitmap.pitch)};

        rv.bitmaps.emplace(std::piecewise_construct,
            std::forward_as_tuple(static_cast<char>(character_code)),
            std::forward_as_tuple(glm::ivec2{current_glyph.bitmap.width,
                                      current_glyph.bitmap.rows},
                glm::ivec2{current_glyph.bitmap_left, current_glyph.bitmap_top},
                bitmap_width,
                static_cast<uint32_t>(current_glyph.advance.x)));

        if (current_glyph.bitmap.width > 0)
        {
            auto bitmap_data{
                individual_bitmaps.emplace(std::piecewise_construct,
                    std::forward_as_tuple(static_cast<char>(character_code)),
                    std::forward_as_tuple(
                        current_glyph.bitmap.width * current_glyph.bitmap.rows,
                        std::byte{0}))};

            for (unsigned int i{}; i != current_glyph.bitmap.rows; ++i)
            {
                for (unsigned int j{}; j != current_glyph.bitmap.width; ++j)
                {
                    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                    bitmap_data.first->second[i * pitch + j] =
                        static_cast<std::byte>(
                            current_glyph.bitmap.buffer[i * pitch + j]);
                    // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                }
            }
        }
        bitmap_width += current_glyph.bitmap.width;
    }

    font_face_guard.reset();

    rv.bitmap_width = bitmap_width;
    rv.bitmap_height = bitmap_height;
    rv.bitmap_data.resize(static_cast<size_t>(bitmap_height) * bitmap_width);

    uint32_t xpos{};
    for (FT_ULong character_code{}; character_code != 128; ++character_code)
    {
        auto const character{static_cast<char>(character_code)};

        auto const& bitmap_data{rv.bitmaps[character]};
        auto const& bitmap{individual_bitmaps[character]};

        // NOLINTBEGIN(cppcoreguidelines-pro-type-union-access)
        auto const width{static_cast<uint32_t>(bitmap_data.size.x)};
        auto const height{static_cast<uint32_t>(bitmap_data.size.y)};
        // NOLINTEND(cppcoreguidelines-pro-type-union-access)
        for (uint32_t i{}; i != height; ++i)
        {
            for (uint32_t j{}; j != width; ++j)
            {
                rv.bitmap_data[i * bitmap_width + xpos + j] =
                    bitmap[i * width + j];
            }
        }
        xpos += width;
    }

    return rv;
}
