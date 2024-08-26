#include <vkrndr_descriptors.hpp>

#include <vkrndr_device.hpp>
#include <vkrndr_utility.hpp>

void vkrndr::create_descriptor_sets(device_t const* const device,
    VkDescriptorSetLayout const layout,
    VkDescriptorPool const descriptor_pool,
    std::span<VkDescriptorSet> descriptor_sets)
{
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = descriptor_pool;
    alloc_info.descriptorSetCount = count_cast(descriptor_sets.size());
    alloc_info.pSetLayouts = &layout;

    check_result(vkAllocateDescriptorSets(device->logical,
        &alloc_info,
        descriptor_sets.data()));
}
