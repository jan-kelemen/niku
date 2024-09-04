#ifndef VKGLTF_FASTGLTF_ADAPTER_INCLUDED
#define VKGLTF_FASTGLTF_ADAPTER_INCLUDED

#include <vkgltf_error.hpp>

#include <fastgltf/core.hpp>

#include <fmt/base.h>

#include <vulkan/vulkan_core.h>

#include <cassert>

namespace vkgltf
{
    [[nodiscard]] constexpr error_t translate_error(fastgltf::Error const err)
    {
        using fastgltf::Error;

        switch (err)
        {
        case Error::None:
            return error_t::none;
        case Error::InvalidPath: // Invalid directory passed to load*GLTF
            return error_t::invalid_file;
        // fastgltf::Parser doesn't enable required extensions
        case Error::MissingExtensions:
        // glTF file requires unsupported extensions
        case Error::UnknownRequiredExtension:
        case Error::InvalidJson:
        case Error::InvalidGltf:
        case Error::InvalidOrMissingAssetField:
        case Error::InvalidGLB:
        case Error::MissingExternalBuffer:
        case Error::UnsupportedVersion:
        case Error::InvalidURI:
        case Error::InvalidFileData:
            return error_t::parse_failed;
        case Error::FailedWritingFiles:
            return error_t::export_failed;
        case Error::FileBufferAllocationFailed:
            return error_t::out_of_memory;
        case Error::MissingField: // Used only internally in fastgltf
        default:
            return error_t::unknown;
        }
    }

    [[nodiscard]] constexpr VkPrimitiveTopology to_vulkan(
        fastgltf::PrimitiveType t)
    {
        using fastgltf::PrimitiveType;

        // NOLINTBEGIN(bugprone-branch-clone)
        switch (t)
        {
        case PrimitiveType::Points:
            return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        case PrimitiveType::Lines:
            return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case PrimitiveType::LineLoop:
            // TODO-JK: This is wrong, LineLoop should connect first and last
            // vertex A different question is if there is any need to support it
            // actually
            return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case PrimitiveType::LineStrip:
            return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case PrimitiveType::Triangles:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case PrimitiveType::TriangleStrip:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case PrimitiveType::TriangleFan:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
        default:
            assert(false);
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        }
        // NOLINTEND(bugprone-branch-clone)

        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    }

} // namespace vkgltf

template<>
struct fmt::formatter<fastgltf::Error>
{
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    format_context::iterator format(fastgltf::Error const& err,
        format_context& ctx) const;
};

#endif
