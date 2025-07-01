#ifndef VKRNDR_DESCRIPTORS_INCLUDED
#define VKRNDR_DESCRIPTORS_INCLUDED

#include <vkrndr_buffer.hpp>
#include <vkrndr_cubemap.hpp>
#include <vkrndr_image.hpp>

#include <volk.h>

#include <expected>
#include <span>

namespace vkrndr
{
    struct device_t;
} // namespace vkrndr

namespace vkrndr
{
    // Descriptor pool

    [[nodiscard]] std::expected<VkDescriptorPool, VkResult>
    create_descriptor_pool(device_t const& device,
        std::span<VkDescriptorPoolSize const> const& pool_sizes,
        uint32_t max_sets,
        VkDescriptorPoolCreateFlags flags =
            VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);

    [[nodiscard]] VkResult allocate_descriptor_sets(device_t const& device,
        VkDescriptorPool pool,
        std::span<VkDescriptorSetLayout const> const& layouts,
        std::span<VkDescriptorSet> descriptor_sets);

    [[nodiscard]] VkResult allocate_descriptor_sets(device_t const& device,
        VkDescriptorPool pool,
        std::span<VkDescriptorSetLayout const> const& layouts,
        std::span<uint32_t const> const& variable_counts,
        std::span<VkDescriptorSet> descriptor_sets);

    void free_descriptor_sets(device_t const& device,
        VkDescriptorPool pool,
        std::span<VkDescriptorSet const> const& descriptor_sets);

    void reset_descriptor_pool(device_t const& device, VkDescriptorPool pool);

    void destroy_descriptor_pool(device_t const& device, VkDescriptorPool pool);

    // Descriptor set layout

    [[nodiscard]] std::expected<VkDescriptorSetLayout, VkResult>
    create_descriptor_set_layout(device_t const& device,
        std::span<VkDescriptorSetLayoutBinding const> const& bindings,
        std::span<VkDescriptorBindingFlagsEXT const> const& binding_flags = {});

    // Descriptor infos

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
