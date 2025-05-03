#include <ngntxt_font_face.hpp>

#include <cppext_numeric.hpp>

#include <boost/scope/scope_exit.hpp>

#include <fmt/format.h>
#include <fmt/std.h> // IWYU pragma: keep

#include <spdlog/spdlog.h>

#include <cassert>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

// IWYU pragma: no_include <fmt/base.h>

std::shared_ptr<ngntxt::freetype_context_t> ngntxt::freetype_context_t::create()
{
    try
    {
        return std::make_shared<freetype_context_t>(private_tag_t{});
    }
    catch (std::runtime_error const& ex)
    {
        spdlog::error(ex.what());
        return nullptr;
    }
}

// NOLINTNEXTLINE
ngntxt::freetype_context_t::freetype_context_t(private_tag_t)
{
    if (FT_Error const error{FT_Init_FreeType(&library_)}; error)
    {
        throw std::runtime_error{
            fmt::format("Unable to initialize FreeType library. Error = {}",
                error)};
    }
}

ngntxt::freetype_context_t::~freetype_context_t()
{
    FT_Done_FreeType(library_);
}

std::shared_ptr<ngntxt::freetype_context_t>
ngntxt::freetype_context_t::instance()
{
    return shared_from_this();
}

FT_Library ngntxt::freetype_context_t::handle() const { return library_; }

ngntxt::font_face_t::font_face_t(std::shared_ptr<freetype_context_t> context,
    FT_Face font_face)
    : context_{std::move(context)}
    , font_face_{font_face}
{
    assert(context_);
}

ngntxt::font_face_t::font_face_t(font_face_t&& other) noexcept
    : context_{std::move(other.context_)}
    , font_face_{std::exchange(other.font_face_, {})}
{
}

ngntxt::font_face_t::~font_face_t() { FT_Done_Face(font_face_); }

FT_Face ngntxt::font_face_t::handle() const { return font_face_; }

ngntxt::font_face_t& ngntxt::font_face_t::operator=(
    font_face_t&& other) noexcept
{
    using std::swap;
    swap(context_, other.context_);
    swap(font_face_, other.font_face_);

    return *this;
}

std::expected<ngntxt::font_face_t, std::error_code> ngntxt::load_font_face(
    std::shared_ptr<freetype_context_t> context,
    std::filesystem::path const& path,
    glm::uvec2 const char_size,
    glm::uvec2 const screen_dpi)
{
    std::error_code ec;
    if (!is_regular_file(path, ec) || ec)
    {
        return std::unexpected{
            std::make_error_code(std::errc::invalid_argument)};
    }

    FT_Face font_face{};
    if (FT_Error const error{FT_New_Face(context->handle(),
            path.string().c_str(),
            0,
            &font_face)})
    {
        spdlog::error("Unable to load font {}. Error = {}", path, error);
        return std::unexpected{
            std::make_error_code(std::errc::invalid_argument)};
    };

    boost::scope::scope_exit destroy_font{
        [&font_face] { FT_Done_Face(font_face); }};

    font_face_t rv{std::move(context), font_face};

    destroy_font.set_active(false);

    if (FT_Error const error{FT_Set_Char_Size(rv.handle(),
            cppext::narrow<FT_F26Dot6>(char_size.x) * 64,
            cppext::narrow<FT_F26Dot6>(char_size.y) * 64,
            screen_dpi.x,
            screen_dpi.y)})
    {
        spdlog::error("Unable to set font size. Error = {}", error);
        return std::unexpected{
            std::make_error_code(std::errc::invalid_argument)};
    }

    return rv;
}
