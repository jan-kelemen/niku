#include <vkrndr_depth_buffer.hpp>

#include <vkrndr_device.hpp>
#include <vkrndr_image.hpp>

#include <volk.h>

#include <array>
#include <optional>
#include <span>
#include <stdexcept>

namespace
{
    [[nodiscard]] std::optional<VkFormat> find_supported_format(
        VkPhysicalDevice const physical_device,
        std::span<VkFormat const> candidates,
        VkImageTiling const tiling,
        VkFormatFeatureFlags const features)
    {
        for (VkFormat format : candidates)
        {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(physical_device,
                format,
                &props);

            if (tiling == VK_IMAGE_TILING_LINEAR &&
                (props.linearTilingFeatures & features) == features)
            {
                return format;
            }

            if (tiling == VK_IMAGE_TILING_OPTIMAL &&
                (props.optimalTilingFeatures & features) == features)
            {
                return format;
            }
        }

        return std::nullopt;
    }

    [[nodiscard]] VkFormat find_depth_format(
        VkPhysicalDevice const physical_device,
        bool const with_stencil_component)
    {
        if (with_stencil_component)
        {
            constexpr std::array candidates{VK_FORMAT_D32_SFLOAT_S8_UINT,
                VK_FORMAT_D24_UNORM_S8_UINT};
            if (auto const format{find_supported_format(physical_device,
                    candidates,
                    VK_IMAGE_TILING_OPTIMAL,
                    VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)})
            {
                return *format;
            }
        }
        else
        {
            constexpr std::array candidates{VK_FORMAT_D32_SFLOAT,
                VK_FORMAT_D32_SFLOAT_S8_UINT,
                VK_FORMAT_D24_UNORM_S8_UINT};
            if (auto const format{find_supported_format(physical_device,
                    candidates,
                    VK_IMAGE_TILING_OPTIMAL,
                    VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)})
            {
                return *format;
            }
        }

        throw std::runtime_error{"Unable to find suitable depth buffer format"};
    }
} // namespace

[[nodiscard]]
bool vkrndr::has_stencil_component(VkFormat format)
{
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
        format == VK_FORMAT_D24_UNORM_S8_UINT;
}

vkrndr::image_t vkrndr::create_depth_buffer(device_t const& device,
    VkExtent2D const extent,
    bool const with_stencil_component,
    std::optional<VkSampleCountFlagBits> sample_count)
{
    VkFormat const depth_format{
        find_depth_format(device.physical, with_stencil_component)};
    return create_image_and_view(device,
        image_2d_create_info_t{.format = depth_format,
            .extent = extent,
            .samples = sample_count.value_or(device.max_msaa_samples),
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            .required_memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
        VK_IMAGE_ASPECT_DEPTH_BIT);
}
