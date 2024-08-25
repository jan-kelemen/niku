#ifndef VKRNDR_VULKAN_DESCRIPTORS_INCLUDED
#define VKRNDR_VULKAN_DESCRIPTORS_INCLUDED

#include <vulkan/vulkan_core.h>

#include <span>

namespace vkrndr
{
    struct vulkan_device;
} // namespace vkrndr

namespace vkrndr
{
    void create_descriptor_sets(vulkan_device const* device,
        VkDescriptorSetLayout layout,
        VkDescriptorPool descriptor_pool,
        std::span<VkDescriptorSet> descriptor_sets);
} // namespace vkrndr

#endif
