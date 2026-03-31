#include <vkrndr_sampler.hpp>

#include <vkrndr_device.hpp>

#include <algorithm>

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
