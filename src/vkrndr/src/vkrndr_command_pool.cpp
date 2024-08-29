#include <vkrndr_command_pool.hpp>

#include <vkrndr_device.hpp>
#include <vkrndr_utility.hpp>

#include <cassert>

vkrndr::command_pool_t::command_pool_t(device_t& device,
    uint32_t const family,
    VkCommandPoolCreateFlags const flags)
    : device_{&device}
{
    VkCommandPoolCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    create_info.queueFamilyIndex = family;
    create_info.flags = flags;

    check_result(
        vkCreateCommandPool(device_->logical, &create_info, nullptr, &pool_));
}

vkrndr::command_pool_t::~command_pool_t()
{
    if (device_)
    {
        vkDestroyCommandPool(device_->logical, pool_, nullptr);
    }
}

void vkrndr::command_pool_t::allocate_command_buffers(bool const primary,
    uint32_t const count,
    std::span<VkCommandBuffer> buffers)
{
    assert(count <= buffers.size());

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = pool_;
    alloc_info.level = primary ? VK_COMMAND_BUFFER_LEVEL_PRIMARY
                               : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    alloc_info.commandBufferCount = count;

    check_result(vkAllocateCommandBuffers(device_->logical,
        &alloc_info,
        buffers.data()));
}

void vkrndr::command_pool_t::free_command_buffers(
    std::span<VkCommandBuffer const> const& buffers)
{
    vkFreeCommandBuffers(device_->logical,
        pool_,
        count_cast(buffers.size()),
        buffers.data());
}

void vkrndr::command_pool_t::reset(bool release_resources)
{
    check_result(vkResetCommandPool(device_->logical,
        pool_,
        release_resources ? VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT : 0));
}

void vkrndr::command_pool_t::trim()
{
    vkTrimCommandPool(device_->logical, pool_, 0);
}
