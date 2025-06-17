#ifndef VKRNDR_DESCRIPTORS_INCLUDED
#define VKRNDR_DESCRIPTORS_INCLUDED

#include <vkrndr_buffer.hpp>
#include <vkrndr_cubemap.hpp>
#include <vkrndr_image.hpp>

#include <volk.h>

#include <span>

namespace vkrndr
{
    struct device_t;
} // namespace vkrndr

namespace vkrndr
{
    [[nodiscard]] VkDescriptorSetLayout create_descriptor_set_layout(
        device_t const& device,
        std::span<VkDescriptorSetLayoutBinding const> const& bindings,
        std::span<VkDescriptorBindingFlagsEXT const> const& binding_flags = {});

    [[nodiscard]] constexpr VkDescriptorBufferInfo buffer_descriptor(
        vkrndr::buffer_t const& buffer,
        VkDeviceSize const offset = 0,
        VkDeviceSize const range = VK_WHOLE_SIZE)
    {
        return {buffer.buffer, offset, range};
    }

    [[nodiscard]] constexpr VkDescriptorImageInfo sampled_image_descriptor(
        image_t const& image,
        VkImageLayout const layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        return {.imageView = image.view, .imageLayout = layout};
    }

    [[nodiscard]] constexpr VkDescriptorImageInfo sampler_descriptor(
        VkSampler sampler)
    {
        return {.sampler = sampler};
    }

    [[nodiscard]] constexpr VkDescriptorImageInfo combined_sampler_descriptor(
        VkSampler sampler,
        image_t const& image,
        VkImageLayout const layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        return {.sampler = sampler,
            .imageView = image.view,
            .imageLayout = layout};
    }

    [[nodiscard]] constexpr VkDescriptorImageInfo combined_sampler_descriptor(
        VkSampler sampler,
        cubemap_t const& cubemap,
        VkImageLayout const layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        return {.sampler = sampler,
            .imageView = cubemap.view,
            .imageLayout = layout};
    }

    [[nodiscard]] constexpr VkDescriptorImageInfo storage_image_descriptor(
        image_t const& image,
        VkImageLayout const layout = VK_IMAGE_LAYOUT_GENERAL)
    {
        return {.imageView = image.view, .imageLayout = layout};
    }
} // namespace vkrndr

#endif
