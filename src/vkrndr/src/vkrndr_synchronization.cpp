#include <vkrndr_synchronization.hpp>

#include <vkrndr_device.hpp>
#include <vkrndr_utility.hpp>

#include <cppext_numeric.hpp>

#include <vulkan/utility/vk_struct_helper.hpp>

#include <cassert>
#include <expected>
#include <utility>

VkSemaphore vkrndr::create_semaphore(device_t const& device)
{
    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkSemaphore rv; // NOLINT
    check_result(vkCreateSemaphore(device, &semaphore_info, nullptr, &rv));

    return rv;
}

VkFence vkrndr::create_fence(device_t const& device, bool const set_signaled)
{
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (set_signaled)
    {
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    }

    VkFence rv; // NOLINT
    check_result(vkCreateFence(device, &fence_info, nullptr, &rv));

    return rv;
}

void vkrndr::wait_for(VkCommandBuffer command_buffer,
    std::span<VkMemoryBarrier2 const> const& global_memory_barriers,
    std::span<VkBufferMemoryBarrier2 const> const& buffer_barriers,
    std::span<VkImageMemoryBarrier2 const> const& image_barriers)
{
    VkDependencyInfo dependency{};
    dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;

    if (!global_memory_barriers.empty())
    {
        dependency.memoryBarrierCount =
            cppext::narrow<uint32_t>(global_memory_barriers.size());
        dependency.pMemoryBarriers = global_memory_barriers.data();
    }

    if (!buffer_barriers.empty())
    {
        dependency.bufferMemoryBarrierCount =
            cppext::narrow<uint32_t>(buffer_barriers.size());
        dependency.pBufferMemoryBarriers = buffer_barriers.data();
    }

    if (!image_barriers.empty())
    {
        dependency.imageMemoryBarrierCount =
            cppext::narrow<uint32_t>(image_barriers.size());
        dependency.pImageMemoryBarriers = image_barriers.data();
    }

    vkCmdPipelineBarrier2(command_buffer, &dependency);
}

vkrndr::semaphore_pool_t::semaphore_pool_t(vkrndr::device_t const& device)
    : device_{&device}
{
}

vkrndr::semaphore_pool_t::~semaphore_pool_t()
{
    for (VkSemaphore const semaphore : pool_)
    {
        vkDestroySemaphore(*device_, semaphore, nullptr);
    }
}

std::expected<VkSemaphore, VkResult> vkrndr::semaphore_pool_t::get()
{
    if (!pool_.empty())
    {
        VkSemaphore const rv{pool_.back()};
        pool_.pop_back();
        return rv;
    }

    VkSemaphoreCreateInfo const create_info{
        .sType = vku::GetSType<VkSemaphoreCreateInfo>(),
    };

    VkSemaphore rv{VK_NULL_HANDLE};
    if (VkResult const result{
            vkCreateSemaphore(*device_, &create_info, nullptr, &rv)};
        result != VK_SUCCESS)
    {
        return std::unexpected{result};
    }

    return rv;
}

VkResult vkrndr::semaphore_pool_t::recycle(VkSemaphore const semaphore)
{
    assert(semaphore != VK_NULL_HANDLE);
    pool_.push_back(semaphore);
    return VK_SUCCESS;
}

vkrndr::semaphore_pool_t& vkrndr::semaphore_pool_t::operator=(
    semaphore_pool_t&& other) noexcept
{
    using std::swap;
    swap(device_, other.device_);
    swap(pool_, other.pool_);

    return *this;
}

vkrndr::fence_pool_t::fence_pool_t(vkrndr::device_t const& device)
    : device_{&device}
{
}

vkrndr::fence_pool_t::~fence_pool_t()
{
    for (VkFence const fence : pool_)
    {
        vkDestroyFence(*device_, fence, nullptr);
    }
}

std::expected<VkFence, VkResult> vkrndr::fence_pool_t::get(bool const signaled)
{
    if (!signaled && !pool_.empty())
    {
        VkFence const rv{pool_.back()};
        pool_.pop_back();
        return rv;
    }

    VkFenceCreateInfo create_info{
        .sType = vku::GetSType<VkFenceCreateInfo>(),
    };

    if (signaled)
    {
        create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    }

    VkFence rv{VK_NULL_HANDLE};
    if (VkResult const result{
            vkCreateFence(*device_, &create_info, nullptr, &rv)};
        result != VK_SUCCESS)
    {
        return std::unexpected{result};
    }

    return rv;
}

VkResult vkrndr::fence_pool_t::recycle(VkFence const fence)
{
    assert(fence != VK_NULL_HANDLE);
    pool_.push_back(fence);

    if (VkResult const result{vkResetFences(*device_, 1, &fence)};
        result != VK_SUCCESS)
    {
        assert(false);
        pool_.pop_back();
        return result;
    }

    return VK_SUCCESS;
}

vkrndr::fence_pool_t& vkrndr::fence_pool_t::operator=(
    fence_pool_t&& other) noexcept
{
    using std::swap;
    swap(device_, other.device_);
    swap(pool_, other.pool_);

    return *this;
}
