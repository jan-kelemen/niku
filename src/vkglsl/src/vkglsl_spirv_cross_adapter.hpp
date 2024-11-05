#ifndef VKGLSL_SPIRV_CROSS_ADAPTER_INCLUDED
#define VKGLSL_SPIRV_CROSS_ADAPTER_INCLUDED

#include <spirv_cross.hpp>

#include <cstdint>
#include <optional>

namespace vkglsl
{
    [[nodiscard]] std::optional<uint32_t> resource_decoration(
        spirv_cross::Compiler const& compiler,
        spirv_cross::Resource const& resource,
        spv::Decoration decoration);
} // namespace vkglsl

#endif
