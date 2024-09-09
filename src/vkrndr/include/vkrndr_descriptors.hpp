#ifndef VKRNDR_DESCRIPTORS_INCLUDED
#define VKRNDR_DESCRIPTORS_INCLUDED

#include <vulkan/vulkan_core.h>

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
} // namespace vkrndr

#endif
