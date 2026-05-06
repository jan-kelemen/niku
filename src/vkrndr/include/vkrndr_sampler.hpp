#ifndef VKRNDR_SAMPLER_INCLUDED
#define VKRNDR_SAMPLER_INCLUDED

#include <cppext_hash.hpp> // IWYU pragma: keep

#include <boost/hash2/hash_append_fwd.hpp> // IWYU pragma: keep

#include <volk.h>

#include <optional>

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
        sampler_properties_t const& properties,
        bool enable_default_anisotropy);

    [[nodiscard]] VkSamplerCreateInfo as_create_info(
        sampler_properties_t const& properties);

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
