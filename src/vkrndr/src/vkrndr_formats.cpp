#include <vkrndr_formats.hpp>

#include <vkrndr_utility.hpp>

#include <cppext_pragma_warning.hpp>

#include <volk.h>

#include <vulkan/utility/vk_struct_helper.hpp>

#include <array>
#include <cassert>
#include <ranges>

namespace
{
    constexpr std::array<vkrndr::depth_stencil_format_properties_t, 7>
        all_depth_stencil_formats{{{VK_FORMAT_D32_SFLOAT, {}, true, false},
            {VK_FORMAT_D32_SFLOAT_S8_UINT, {}, true, true},
            {VK_FORMAT_D24_UNORM_S8_UINT, {}, true, true},
            {VK_FORMAT_X8_D24_UNORM_PACK32, {}, true, false},
            {VK_FORMAT_D16_UNORM, {}, true, false},
            {VK_FORMAT_D16_UNORM_S8_UINT, {}, true, true},
            {VK_FORMAT_S8_UINT, {}, false, true}}};

    [[nodiscard]] bool format_supported(VkPhysicalDevice const device,
        VkFormat const format)
    {
        auto properties{vku::InitStruct<VkFormatProperties2>()};
        vkGetPhysicalDeviceFormatProperties2(device, format, &properties);

        return vkrndr::supports_flags(
            properties.formatProperties.optimalTilingFeatures,
            VK_FORMAT_FEATURE_TRANSFER_DST_BIT,
            VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
    }
} // namespace

std::vector<vkrndr::depth_stencil_format_properties_t>
vkrndr::find_supported_depth_stencil_formats(VkPhysicalDevice const device,
    bool const needs_depth_component,
    bool const needs_stencil_component)
{
    assert(needs_depth_component || needs_stencil_component);

    DISABLE_WARNING_PUSH
    DISABLE_WARNING_NRVO
    auto const filter_formats = [&device](auto&& func)
    {
        std::vector<depth_stencil_format_properties_t> rv;
        for (auto const& format :
            std::views::filter(all_depth_stencil_formats, func))
        {
            auto properties{vku::InitStruct<VkFormatProperties2>()};
            vkGetPhysicalDeviceFormatProperties2(device,
                format.format,
                &properties);

            if (vkrndr::supports_flags(
                    properties.formatProperties.linearTilingFeatures,
                    VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) ||
                vkrndr::supports_flags(
                    properties.formatProperties.optimalTilingFeatures,
                    VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
            {
                rv.emplace_back(format.format,
                    properties.formatProperties,
                    format.depth_component,
                    format.stencil_component);
            }
        }

        return rv;
    };

    if (needs_depth_component && needs_stencil_component)
    {
        return filter_formats([](auto const& f)
            { return f.depth_component && f.stencil_component; });
    }

    if (needs_depth_component)
    {
        return filter_formats([](auto const& f) { return f.depth_component; });
    }

    return filter_formats([](auto const& f) { return f.stencil_component; });
    DISABLE_WARNING_POP
}

std::vector<VkFormat> vkrndr::find_supported_texture_compression_formats(
    VkPhysicalDevice device)
{
    std::vector<VkFormat> rv;

    auto features{vku::InitStruct<VkPhysicalDeviceFeatures2>()};
    vkGetPhysicalDeviceFeatures2(device, &features);

    auto const check_and_add_if_available = [&device, &rv](
                                                VkFormat const format)
    {
        if (format_supported(device, format))
        {
            rv.emplace_back(format);
        }
    };

    if (features.features.textureCompressionBC)
    {
        check_and_add_if_available(VK_FORMAT_BC7_SRGB_BLOCK);
        check_and_add_if_available(VK_FORMAT_BC7_UNORM_BLOCK);
        check_and_add_if_available(VK_FORMAT_BC3_SRGB_BLOCK);
        check_and_add_if_available(VK_FORMAT_BC3_UNORM_BLOCK);
    }

    if (features.features.textureCompressionASTC_LDR)
    {
        check_and_add_if_available(VK_FORMAT_ASTC_4x4_SRGB_BLOCK);
        check_and_add_if_available(VK_FORMAT_ASTC_4x4_UNORM_BLOCK);
    }

    if (features.features.textureCompressionETC2)
    {
        check_and_add_if_available(VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK);
        check_and_add_if_available(VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK);
    }

    check_and_add_if_available(VK_FORMAT_R8G8B8A8_SRGB);
    check_and_add_if_available(VK_FORMAT_R8G8B8A8_UNORM);

    assert(!rv.empty());

    return rv;
}
