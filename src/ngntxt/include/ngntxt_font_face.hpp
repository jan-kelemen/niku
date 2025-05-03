#ifndef NGNTXT_FONT_FACE_INCLUDED
#define NGNTXT_FONT_FACE_INCLUDED

#include <ft2build.h>
#include FT_FREETYPE_H // IWYU pragma: keep

#include <glm/vec2.hpp>

#include <expected>
#include <filesystem>
#include <memory>
#include <system_error>

namespace ngntxt
{
    class [[nodiscard]] freetype_context_t final
        : public std::enable_shared_from_this<freetype_context_t>
    {
        struct private_tag_t
        {
            explicit private_tag_t() = default;
        };

    public:
        [[nodiscard]] static std::shared_ptr<freetype_context_t> create();

    public:
        // NOLINTNEXTLINE(readability-named-parameter)
        explicit freetype_context_t(private_tag_t);

        freetype_context_t(freetype_context_t const&) = default;

        freetype_context_t(freetype_context_t&&) noexcept = delete;

    public:
        ~freetype_context_t();

    public:
        [[nodiscard]] std::shared_ptr<freetype_context_t> instance();

        [[nodiscard]] FT_Library handle() const;

    public:
        freetype_context_t& operator=(freetype_context_t const&) = default;

        freetype_context_t& operator=(freetype_context_t&&) noexcept = delete;

    private:
        FT_Library library_;
    };

    class [[nodiscard]] font_face_t final
    {
    public:
        font_face_t(std::shared_ptr<freetype_context_t> context,
            FT_Face font_face);

        font_face_t(font_face_t const&) = delete;

        font_face_t(font_face_t&& other) noexcept;

    public:
        ~font_face_t();

    public:
        [[nodiscard]] FT_Face handle() const;

    public:
        font_face_t& operator=(font_face_t const&) = delete;

        font_face_t& operator=(font_face_t&& other) noexcept;

    private:
        std::shared_ptr<freetype_context_t> context_;
        FT_Face font_face_;
    };

    std::expected<font_face_t, std::error_code> load_font_face(
        std::shared_ptr<freetype_context_t> context,
        std::filesystem::path const& path,
        glm::uvec2 char_size,
        glm::uvec2 screen_dpi);
} // namespace ngntxt
#endif
