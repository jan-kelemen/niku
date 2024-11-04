#include <vkglsl_spirv_cross_adapter.hpp>

#include <spirv_cross.hpp>

#include <cstdint>
#include <optional>

[[nodiscard]]
std::optional<uint32_t> vkglsl::resource_decoration(
    spirv_cross::Compiler const& compiler,
    spirv_cross::Resource const& resource,
    spv::Decoration const decoration)
{
    if (!compiler.has_decoration(resource.id, decoration))
    {
        return std::nullopt;
    }

    return std::make_optional(compiler.get_decoration(resource.id, decoration));
}
