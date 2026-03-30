#ifndef VKRNDR_SAMPLER_INCLUDED
#define VKRNDR_SAMPLER_INCLUDED

#include <vkrndr_utility.hpp>

#include <cppext_hash.hpp> // IWYU pragma: keep

#include <boost/hash2/hash_append_fwd.hpp> // IWYU pragma: keep

#include <volk.h>

#include <cassert>
#include <optional>

#ifndef NDEBUG
#include <cmath>
#endif

// IWYU pragma: no_forward_declare boost::hash2::hash_append_tag

namespace vkrndr
{
    struct device_t;
} // namespace vkrndr

namespace vkrndr
{
    struct [[nodiscard]] sampler_properties_t final
    {
        VkFilter magnification_filter{VK_FILTER_LINEAR};
        VkFilter minification_filter{VK_FILTER_LINEAR};
        VkSamplerMipmapMode mipmap_mode{VK_SAMPLER_MIPMAP_MODE_LINEAR};
        VkSamplerAddressMode address_mode_u{VK_SAMPLER_ADDRESS_MODE_REPEAT};
        VkSamplerAddressMode address_mode_v{VK_SAMPLER_ADDRESS_MODE_REPEAT};
        VkSamplerAddressMode address_mode_w{VK_SAMPLER_ADDRESS_MODE_REPEAT};
        float mip_lod_bias{0.0f};
        std::optional<float> max_anisotropy;
        std::optional<VkCompareOp> compare_op;
        float min_lod{0.0f};
        float max_lod{VK_LOD_CLAMP_NONE};
        VkBorderColor border_color{VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK};
        bool unnormalized_coordinates{false};

        friend constexpr bool operator==(sampler_properties_t const&,
            sampler_properties_t const&) noexcept = default;
    };

    [[nodiscard]] VkSamplerCreateInfo as_create_info(
        vkrndr::device_t const& device,
        sampler_properties_t const& properties);

    [[nodiscard]] constexpr VkSamplerCreateInfo as_create_info(
        sampler_properties_t const& properties)
    {
        assert(std::isfinite(properties.mip_lod_bias));
        assert(std::isfinite(properties.min_lod));
        assert(std::isfinite(properties.max_lod));
        assert(!properties.max_anisotropy.has_value() ||
            std::isfinite(*properties.max_anisotropy));

        return {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = properties.magnification_filter,
            .minFilter = properties.minification_filter,
            .mipmapMode = properties.mipmap_mode,
            .addressModeU = properties.address_mode_u,
            .addressModeV = properties.address_mode_v,
            .addressModeW = properties.address_mode_w,
            .mipLodBias = properties.mip_lod_bias,
            .anisotropyEnable = to_bool(properties.max_anisotropy),
            .maxAnisotropy = properties.max_anisotropy.value_or(1.0f),
            .compareEnable = to_bool(properties.compare_op),
            .compareOp = properties.compare_op.value_or(VK_COMPARE_OP_NEVER),
            .minLod = properties.min_lod,
            .maxLod = properties.max_lod,
            .borderColor = properties.border_color,
            .unnormalizedCoordinates =
                to_bool(properties.unnormalized_coordinates),
        };
    }

    template<typename Provider, typename Hash, typename Flavor>
    constexpr void tag_invoke(boost::hash2::hash_append_tag const& /* tag */,
        Provider const& pr,
        Hash& h,
        Flavor const& f,
        sampler_properties_t const* const v)
    {
        pr.hash_append(h, f, v->magnification_filter);
        pr.hash_append(h, f, v->minification_filter);
        pr.hash_append(h, f, v->mipmap_mode);
        pr.hash_append(h, f, v->address_mode_u);
        pr.hash_append(h, f, v->address_mode_v);
        pr.hash_append(h, f, v->address_mode_w);
        pr.hash_append(h, f, v->mip_lod_bias);
        pr.hash_append(h, f, v->max_anisotropy);
        pr.hash_append(h, f, v->compare_op);
        pr.hash_append(h, f, v->min_lod);
        pr.hash_append(h, f, v->max_lod);
        pr.hash_append(h, f, v->border_color);
        pr.hash_append(h, f, v->unnormalized_coordinates);
    }
} // namespace vkrndr

#endif
