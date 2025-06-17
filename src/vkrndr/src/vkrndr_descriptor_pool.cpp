#include <vkrndr_descriptor_pool.hpp>

#include <vkrndr_device.hpp>
#include <vkrndr_utility.hpp>

#include <cassert>
#include <utility>

vkrndr::descriptor_pool_t::descriptor_pool_t(device_t& device,
    VkDescriptorPool pool)
    : device_{&device}
    , pool_{pool}
{
}

vkrndr::descriptor_pool_t::descriptor_pool_t(descriptor_pool_t&& other) noexcept
    : device_{std::exchange(other.device_, nullptr)}
    , pool_{std::exchange(other.pool_, VK_NULL_HANDLE)}
{
}

vkrndr::descriptor_pool_t::~descriptor_pool_t()
{
    if (device_)
    {
        vkDestroyDescriptorPool(device_->logical, pool_, nullptr);
    }
}

VkResult vkrndr::descriptor_pool_t::allocate_descriptor_sets(
    std::span<VkDescriptorSetLayout const> const& layouts,
    std::span<VkDescriptorSet> descriptor_sets)
{
    assert(descriptor_sets.size() >= layouts.size());

    VkDescriptorSetAllocateInfo const alloc_info{
        .sType = vku::GetSType<VkDescriptorSetAllocateInfo>(),
        .descriptorPool = pool_,
        .descriptorSetCount = count_cast(layouts.size()),
        .pSetLayouts = layouts.data(),
    };

    return vkAllocateDescriptorSets(device_->logical,
        &alloc_info,
        descriptor_sets.data());
}

void vkrndr::descriptor_pool_t::free_descriptor_sets(
    std::span<VkDescriptorSet const> const& descriptor_sets)
{
    if (descriptor_sets.empty())
    {
        return;
    }

    vkFreeDescriptorSets(device_->logical,
        pool_,
        count_cast(descriptor_sets.size()),
        descriptor_sets.data());
}

void vkrndr::descriptor_pool_t::reset()
{
    vkResetDescriptorPool(device_->logical, pool_, {});
}

vkrndr::descriptor_pool_t& vkrndr::descriptor_pool_t::operator=(
    descriptor_pool_t&& other) noexcept
{
    using std::swap;

    swap(device_, other.device_);
    swap(pool_, other.pool_);

    return *this;
}

std::expected<vkrndr::descriptor_pool_t, VkResult>
vkrndr::create_descriptor_pool(device_t& device,
    std::span<VkDescriptorPoolSize const> const& pool_sizes,
    uint32_t const max_sets,
    VkDescriptorPoolCreateFlags const flags)
{
    VkDescriptorPoolCreateInfo const create_info{
        .sType = vku::GetSType<VkDescriptorPoolCreateInfo>(),
        .flags = flags,
        .maxSets = max_sets,
        .poolSizeCount = vkrndr::count_cast(pool_sizes.size()),
        .pPoolSizes = pool_sizes.data(),
    };

    VkDescriptorPool pool{};
    if (auto const result{vkCreateDescriptorPool(device.logical,
            &create_info,
            nullptr,
            &pool)};
        result != VK_SUCCESS)
    {
        return std::unexpected(result);
    }

    return vkrndr::descriptor_pool_t{device, pool};
}
