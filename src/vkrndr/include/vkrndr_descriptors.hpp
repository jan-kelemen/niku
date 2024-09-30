#ifndef VKRNDR_DESCRIPTORS_INCLUDED
#define VKRNDR_DESCRIPTORS_INCLUDED

#include <vkrndr_buffer.hpp>
#include <vkrndr_image.hpp>

#include <cppext_pragma_warning.hpp>

#include <volk.h>

#include <span>

namespace vkrndr
{
    struct device_t;
} // namespace vkrndr

namespace vkrndr
{
    void create_descriptor_sets(device_t const& device,
        VkDescriptorPool descriptor_pool,
        std::span<VkDescriptorSetLayout const> const& layouts,
        std::span<VkDescriptorSet> descriptor_sets);

    void free_descriptor_sets(device_t const& device,
        VkDescriptorPool descriptor_pool,
        std::span<VkDescriptorSet> descriptor_sets);

    [[nodiscard]] constexpr VkDescriptorBufferInfo buffer_descriptor(
        vkrndr::buffer_t const& buffer,
        VkDeviceSize const offset = 0,
        VkDeviceSize const range = VK_WHOLE_SIZE)
    {
        return {buffer.buffer, offset, range};
    }

    [[nodiscard]] constexpr VkDescriptorImageInfo sampled_image_descriptor(
        vkrndr::image_t const& image,
        VkImageLayout const layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        DISABLE_WARNING_PUSH
        DISABLE_WARNING_MISSING_FIELD_INITIALIZERS

        return {.imageView = image.view, .imageLayout = layout};

        DISABLE_WARNING_POP
    }

    [[nodiscard]] constexpr VkDescriptorImageInfo sampler_descriptor(
        VkSampler sampler)
    {
        DISABLE_WARNING_PUSH
        DISABLE_WARNING_MISSING_FIELD_INITIALIZERS

        return {.sampler = sampler};

        DISABLE_WARNING_POP
    }

    [[nodiscard]] constexpr VkDescriptorImageInfo combined_sampler_descriptor(
        VkSampler sampler,
        vkrndr::image_t const& image,
        VkImageLayout const layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        return {.sampler = sampler,
            .imageView = image.view,
            .imageLayout = layout};
    }
} // namespace vkrndr

#endif
