#include <vkrndr_descriptors.hpp>

#include <vkrndr_device.hpp>
#include <vkrndr_utility.hpp>

#include <cassert>

void vkrndr::create_descriptor_sets(device_t const& device,
    VkDescriptorPool descriptor_pool,
    std::span<VkDescriptorSetLayout const> const& layouts,
    std::span<VkDescriptorSet> descriptor_sets)
{
    assert(layouts.size() == descriptor_sets.size());

    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = descriptor_pool;
    alloc_info.descriptorSetCount = count_cast(descriptor_sets.size());
    alloc_info.pSetLayouts = layouts.data();

    check_result(vkAllocateDescriptorSets(device.logical,
        &alloc_info,
        descriptor_sets.data()));
}

void vkrndr::free_descriptor_sets(device_t const& device,
    VkDescriptorPool descriptor_pool,
    std::span<VkDescriptorSet> descriptor_sets)
{
    if (descriptor_sets.empty())
    {
        return;
    }

    vkFreeDescriptorSets(device.logical,
        descriptor_pool,
        count_cast(descriptor_sets.size()),
        descriptor_sets.data());
}

VkDescriptorSetLayout vkrndr::create_descriptor_set_layout(
    device_t const& device,
    std::span<VkDescriptorSetLayoutBinding const> const& bindings)
{
    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = count_cast(bindings.size());
    layout_info.pBindings = bindings.data();

    VkDescriptorSetLayout rv; // NOLINT
    check_result(vkCreateDescriptorSetLayout(device.logical,
        &layout_info,
        nullptr,
        &rv));

    return rv;
}
