#ifndef NGNTXT_FONT_FACE_INCLUDED
#define NGNTXT_FONT_FACE_INCLUDED

#include <boost/smart_ptr/intrusive_ptr.hpp>

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

    using font_face_ptr_t = boost::intrusive_ptr<FT_FaceRec_>;

    std::expected<font_face_ptr_t, std::error_code> load_font_face(
        std::shared_ptr<freetype_context_t> const& context,
        std::filesystem::path const& path,
        glm::uvec2 char_size);
} // namespace ngntxt

void intrusive_ptr_add_ref(FT_FaceRec_* p);

void intrusive_ptr_release(FT_FaceRec_* p);
#endif
