#include <vkrndr_descriptors.hpp>

#include <vkrndr_device.hpp>
#include <vkrndr_utility.hpp>

#include <cassert>

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
