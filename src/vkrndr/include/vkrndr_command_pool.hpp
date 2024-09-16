#ifndef VKRNDR_COMMAND_POOL_INCLUDED
#define VKRNDR_COMMAND_POOL_INCLUDED

#include <volk.h>

#include <cstdint>
#include <span>

namespace vkrndr
{
    struct device_t;
} // namespace vkrndr

namespace vkrndr
{
    class [[nodiscard]] command_pool_t final
    {
    public:
        command_pool_t() = default;

        command_pool_t(device_t& device,
            uint32_t family,
            VkCommandPoolCreateFlags flags = 0);

        command_pool_t(command_pool_t const&) = delete;

        command_pool_t(command_pool_t&&) noexcept = delete;

    public:
        ~command_pool_t();

    public:
        void allocate_command_buffers(bool primary,
            uint32_t count,
            std::span<VkCommandBuffer> buffers);

        void free_command_buffers(
            std::span<VkCommandBuffer const> const& buffers);

        void reset(bool release_resources = false);

        void trim();

    public:
        command_pool_t& operator=(command_pool_t const&) = delete;

        command_pool_t& operator=(command_pool_t&&) noexcept = delete;

    private:
        device_t* device_{nullptr};
        VkCommandPool pool_{VK_NULL_HANDLE};
    };
} // namespace vkrndr

#endif
