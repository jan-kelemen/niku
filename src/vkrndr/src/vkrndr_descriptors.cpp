#include <vkrndr_descriptors.hpp>

#include <vkrndr_device.hpp>
#include <vkrndr_utility.hpp>

#include <vulkan/utility/vk_struct_helper.hpp>

#include <algorithm>
#include <cassert>

std::expected<VkDescriptorPool, VkResult> vkrndr::create_descriptor_pool(
    device_t const& device,
    std::span<VkDescriptorPoolSize const> const& pool_sizes,
    uint32_t max_sets,
    VkDescriptorPoolCreateFlags flags)
{
    VkDescriptorPoolCreateInfo const create_info{
        .sType = vku::GetSType<VkDescriptorPoolCreateInfo>(),
        .flags = flags,
        .maxSets = max_sets,
        .poolSizeCount = vkrndr::count_cast(pool_sizes.size()),
        .pPoolSizes = pool_sizes.data(),
    };

    VkDescriptorPool rv{};
    if (auto const result{
            vkCreateDescriptorPool(device.logical, &create_info, nullptr, &rv)};
        result != VK_SUCCESS)
    {
        return std::unexpected{result};
    }

    return rv;
}

VkResult vkrndr::allocate_descriptor_sets(device_t const& device,
    VkDescriptorPool pool,
    std::span<VkDescriptorSetLayout const> const& layouts,
    std::span<VkDescriptorSet> descriptor_sets)
{
    assert(descriptor_sets.size() >= layouts.size());

    VkDescriptorSetAllocateInfo const alloc_info{
        .sType = vku::GetSType<VkDescriptorSetAllocateInfo>(),
        .descriptorPool = pool,
        .descriptorSetCount = count_cast(layouts.size()),
        .pSetLayouts = layouts.data(),
    };

    return vkAllocateDescriptorSets(device.logical,
        &alloc_info,
        descriptor_sets.data());
}

VkResult vkrndr::allocate_descriptor_sets(device_t const& device,
    VkDescriptorPool pool,
    std::span<VkDescriptorSetLayout const> const& layouts,
    std::span<uint32_t const> const& variable_counts,
    std::span<VkDescriptorSet> descriptor_sets)
{
    assert(descriptor_sets.size() >= layouts.size());

    VkDescriptorSetAllocateInfo alloc_info{
        .sType = vku::GetSType<VkDescriptorSetAllocateInfo>(),
        .descriptorPool = pool,
        .descriptorSetCount = count_cast(layouts.size()),
        .pSetLayouts = layouts.data(),
    };

    VkDescriptorSetVariableDescriptorCountAllocateInfo variable_info{
        .sType =
            vku::GetSType<VkDescriptorSetVariableDescriptorCountAllocateInfo>(),
    };
    if (!variable_counts.empty())
    {
        variable_info.descriptorSetCount = count_cast(variable_counts.size());
        variable_info.pDescriptorCounts = variable_counts.data();

        alloc_info.pNext = &variable_info;
    }

    return vkAllocateDescriptorSets(device.logical,
        &alloc_info,
        descriptor_sets.data());
}

void vkrndr::free_descriptor_sets(device_t const& device,
    VkDescriptorPool pool,
    std::span<VkDescriptorSet const> const& descriptor_sets)
{
    if (descriptor_sets.empty())
    {
        return;
    }

    vkFreeDescriptorSets(device.logical,
        pool,
        count_cast(descriptor_sets.size()),
        descriptor_sets.data());
}

void vkrndr::reset_descriptor_pool(device_t const& device,
    VkDescriptorPool pool)
{
    vkResetDescriptorPool(device.logical, pool, {});
}

void vkrndr::destroy_descriptor_pool(device_t const& device,
    VkDescriptorPool pool)
{
    vkDestroyDescriptorPool(device.logical, pool, nullptr);
}

std::expected<VkDescriptorSetLayout, VkResult>
vkrndr::create_descriptor_set_layout(device_t const& device,
    std::span<VkDescriptorSetLayoutBinding const> const& bindings,
    std::span<VkDescriptorBindingFlagsEXT const> const& binding_flags)
{
    assert(binding_flags.empty() || bindings.size() == binding_flags.size());

    VkDescriptorSetLayoutCreateInfo layout_info{
        .sType = vku::GetSType<VkDescriptorSetLayoutCreateInfo>(),
        .bindingCount = count_cast(bindings.size()),
        .pBindings = bindings.data(),
    };

    VkDescriptorSetLayoutBindingFlagsCreateInfo flags_info{
        .sType = vku::GetSType<VkDescriptorSetLayoutBindingFlagsCreateInfo>(),
    };

    if (std::ranges::any_of(binding_flags,
            [](VkDescriptorBindingFlagsEXT f)
            { return f & VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT; }))
    {
        flags_info.bindingCount = count_cast(binding_flags.size());
        flags_info.pBindingFlags = binding_flags.data();

        layout_info.flags =
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        layout_info.pNext = &flags_info;
    }

    VkDescriptorSetLayout rv; // NOLINT
    check_result(vkCreateDescriptorSetLayout(device.logical,
        &layout_info,
        nullptr,
        &rv));

    return rv;
}
