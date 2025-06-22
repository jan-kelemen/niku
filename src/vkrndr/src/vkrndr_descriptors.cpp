#include <vkrndr_descriptors.hpp>

#include <vkrndr_device.hpp>
#include <vkrndr_utility.hpp>

#include <vulkan/utility/vk_struct_helper.hpp>

#include <algorithm>
#include <cassert>

VkDescriptorSetLayout vkrndr::create_descriptor_set_layout(
    device_t const& device,
    std::span<VkDescriptorSetLayoutBinding const> const& bindings,
    std::span<VkDescriptorBindingFlagsEXT const> const& binding_flags)
{
    assert(binding_flags.empty() || bindings.size() == binding_flags.size());

    VkDescriptorSetLayoutCreateInfo layout_info{
        .sType = vku::GetSType<VkDescriptorSetLayoutCreateInfo>(),
        .bindingCount = count_cast(bindings.size()),
        .pBindings = bindings.data(),
    };

    VkDescriptorSetLayoutBindingFlagsCreateInfoEXT const flags_info{
        .sType =
            vku::GetSType<VkDescriptorSetLayoutBindingFlagsCreateInfoEXT>(),
        .bindingCount = count_cast(binding_flags.size()),
        .pBindingFlags = binding_flags.data(),
    };

    if (binding_flags.empty())
    {
        if (std::ranges::any_of(binding_flags,
                [](VkDescriptorBindingFlagsEXT f)
                { return f & VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT; }))
        {
            layout_info.flags =
                VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        }
        layout_info.pNext = &flags_info;
    }

    VkDescriptorSetLayout rv; // NOLINT
    check_result(vkCreateDescriptorSetLayout(device.logical,
        &layout_info,
        nullptr,
        &rv));

    return rv;
}
