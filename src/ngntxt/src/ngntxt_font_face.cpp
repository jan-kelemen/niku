#include <ngntxt_font_face.hpp>

#include <boost/scope/scope_exit.hpp>

#include <fmt/format.h>
#include <fmt/std.h> // IWYU pragma: keep

#include <spdlog/spdlog.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>

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

std::expected<ngntxt::font_face_ptr_t, std::error_code> ngntxt::load_font_face(
    std::shared_ptr<freetype_context_t> const& context,
    std::filesystem::path const& path,
    glm::uvec2 const char_size)
{
    std::error_code ec;
    if (!is_regular_file(path, ec) || ec)
    {
        return std::unexpected{
            std::make_error_code(std::errc::invalid_argument)};
    }

    FT_Face temp{};
    if (FT_Error const error{
            FT_New_Face(context->handle(), path.string().c_str(), 0, &temp)})
    {
        spdlog::error("Unable to load font {}. Error = {}", path, error);
        return std::unexpected{
            std::make_error_code(std::errc::invalid_argument)};
    };
    boost::scope::scope_exit destroy_font{[temp] { FT_Done_Face(temp); }};

    font_face_ptr_t rv{temp, false};
    destroy_font.set_active(false);

    if (FT_Error const error{
            FT_Set_Pixel_Sizes(rv.get(), char_size.x, char_size.y)})
    {
        spdlog::error("Unable to set font size. Error = {}", error);
        return std::unexpected{
            std::make_error_code(std::errc::invalid_argument)};
    }

    return rv;
}

void intrusive_ptr_add_ref(FT_FaceRec_* const p) { FT_Reference_Face(p); }

void intrusive_ptr_release(FT_FaceRec_* const p) { FT_Done_Face(p); }
