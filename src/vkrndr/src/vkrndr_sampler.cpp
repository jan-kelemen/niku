#include <vkrndr_sampler.hpp>

#include <vkrndr_device.hpp>
#include <vkrndr_utility.hpp>

#include <algorithm>
#include <cassert>
#include <optional>

#ifndef NDEBUG
#include <cmath>
#endif

VkSamplerCreateInfo vkrndr::as_create_info(
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
        .unnormalizedCoordinates = to_bool(properties.unnormalized_coordinates),
    };
}

VkSamplerCreateInfo vkrndr::as_create_info(vkrndr::device_t const& device,
    sampler_properties_t const& properties,
    bool const enable_default_anisotropy)
{
    VkSamplerCreateInfo rv{as_create_info(properties)};

    VkPhysicalDeviceProperties device_properties; // NOLINT
    vkGetPhysicalDeviceProperties(device, &device_properties);

    if (properties.max_anisotropy)
    {
        rv.maxAnisotropy = std::clamp(rv.maxAnisotropy,
            1.0f,
            device_properties.limits.maxSamplerAnisotropy);
    }
    else if (enable_default_anisotropy)
    {
        rv.anisotropyEnable = VK_TRUE;
        rv.maxAnisotropy = device_properties.limits.maxSamplerAnisotropy;
    }

    return rv;
}
