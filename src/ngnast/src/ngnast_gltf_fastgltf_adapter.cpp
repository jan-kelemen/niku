#include <ngnast_gltf_fastgltf_adapter.hpp>

#include <fastgltf/core.hpp>

// IWYU pragma: no_include <string_view>

fmt::format_context::iterator fmt::formatter<fastgltf::Error>::format(
    fastgltf::Error const& err,
    format_context& ctx) const
{
    return fmt::format_to(ctx.out(),
        "{} {}",
        getErrorName(err),
        getErrorMessage(err));
}
