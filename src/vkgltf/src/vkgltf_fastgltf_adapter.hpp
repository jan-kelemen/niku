#ifndef VKGLTF_FASTGLTF_ADAPTER_INCLUDED
#define VKGLTF_FASTGLTF_ADAPTER_INCLUDED

#include <vkgltf_error.hpp>
#include <vkgltf_model.hpp>

#include <fastgltf/core.hpp>
#include <fastgltf/math.hpp> // IWYU pragma: keep
#include <fastgltf/types.hpp>

#include <fmt/base.h>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <volk.h>

#include <cassert>
#include <cstddef>

// IWYU pragma: no_include <glm/detail/qualifier.hpp>

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
        fastgltf::PrimitiveType const t)
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
    }

    [[nodiscard]] constexpr VkFilter to_vulkan(fastgltf::Filter const f)
    {
        using fastgltf::Filter;

        switch (f)
        {
        case Filter::Linear:
        case Filter::LinearMipMapNearest:
        case Filter::LinearMipMapLinear:
            return VK_FILTER_LINEAR;
        case Filter::NearestMipMapNearest:
        case Filter::NearestMipMapLinear:
        case Filter::Nearest:
            return VK_FILTER_NEAREST;
        default:
            assert(false);
            return VK_FILTER_NEAREST;
        }
    }

    [[nodiscard]] constexpr VkSamplerMipmapMode to_vulkan_mipmap(
        fastgltf::Filter const f)
    {
        using fastgltf::Filter;

        switch (f)
        {
        case Filter::Linear:
        case Filter::LinearMipMapLinear:
        case Filter::NearestMipMapLinear:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        case Filter::Nearest:
        case Filter::LinearMipMapNearest:
        case Filter::NearestMipMapNearest:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        default:
            assert(false);
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        }
    }

    [[nodiscard]] constexpr VkSamplerAddressMode to_vulkan(
        fastgltf::Wrap const w)
    {
        using fastgltf::Wrap;

        switch (w)
        {
        case Wrap::Repeat:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case Wrap::MirroredRepeat:
            return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case Wrap::ClampToEdge:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        default:
            assert(false);
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        }
    }

    [[nodiscard]] constexpr alpha_mode_t to_alpha_mode(
        fastgltf::AlphaMode const a)
    {
        using fastgltf::AlphaMode;

        switch (a)
        {
        case AlphaMode::Opaque:
            return vkgltf::alpha_mode_t::opaque;
        case AlphaMode::Mask:
            return vkgltf::alpha_mode_t::mask;
        case AlphaMode::Blend:
            return vkgltf::alpha_mode_t::blend;
        default:
            assert(false);
            return vkgltf::alpha_mode_t::opaque;
        }
    }

    template<typename T, size_t N>
    [[nodiscard]] constexpr glm::vec<N, T> to_glm(
        fastgltf::math::vec<T, N> const& vec)
    {
        if constexpr (N == 2)
        {
            return glm::make_vec2(vec.data());
        }
        else if constexpr (N == 3)
        {
            return glm::make_vec3(vec.data());
        }
        else if constexpr (N == 4)
        {
            return glm::make_vec4(vec.data());
        }
        else
        {
            static_assert(false);
        }
    }

    template<typename T>
    [[nodiscard]] constexpr glm::tquat<T> to_glm(
        fastgltf::math::quat<T> const& quat)
    {
        return glm::make_quat(quat.value_ptr());
    }

    template<typename T, size_t N, size_t M>
    [[nodiscard]] constexpr glm::mat<N, M, T> to_glm(
        fastgltf::math::mat<T, N, M> const& mat)
    {
        if constexpr (N == 2)
        {
            if constexpr (M == 2)
            {
                return glm::make_mat2x2(mat.data());
            }
            else if constexpr (M == 3)
            {
                return glm::make_mat2x3(mat.data());
            }
            else if constexpr (M == 4)
            {
                return glm::make_mat2x4(mat.data());
            }
            else
            {
                static_assert(false);
            }
        }
        else if constexpr (N == 3)
        {
            if constexpr (M == 2)
            {
                return glm::make_mat3x2(mat.data());
            }
            else if constexpr (M == 3)
            {
                return glm::make_mat3x3(mat.data());
            }
            else if constexpr (M == 4)
            {
                return glm::make_mat3x4(mat.data());
            }
            else
            {
                static_assert(false);
            }
        }
        else if constexpr (N == 4)
        {
            if constexpr (M == 2)
            {
                return glm::make_mat4x2(mat.data());
            }
            else if constexpr (M == 3)
            {
                return glm::make_mat4x3(mat.data());
            }
            else if constexpr (M == 4)
            {
                return glm::make_mat4x4(mat.data());
            }
            else
            {
                static_assert(false);
            }
        }
        else
        {
            static_assert(false);
        }
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
