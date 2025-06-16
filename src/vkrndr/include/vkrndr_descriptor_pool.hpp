#ifndef VKRNDR_DESCRIPTOR_POOL_INCLUDED
#define VKRNDR_DESCRIPTOR_POOL_INCLUDED

#include <volk.h>

#include <expected>
#include <span>

namespace vkrndr
{
    struct device_t;
} // namespace vkrndr

namespace vkrndr
{
    class [[nodiscard]] descriptor_pool_t final
    {
    public:
        descriptor_pool_t() = default;

        descriptor_pool_t(device_t& device, VkDescriptorPool pool);

        descriptor_pool_t(descriptor_pool_t const&) = delete;

        descriptor_pool_t(descriptor_pool_t&& other) noexcept;

    public:
        ~descriptor_pool_t();

    public:
        [[nodiscard]] VkResult allocate_descriptor_sets(
            std::span<VkDescriptorSetLayout const> const& layouts,
            std::span<VkDescriptorSet> descriptor_sets);

        void free_descriptor_sets(
            std::span<VkDescriptorSet const> const& descriptor_sets);

        void reset();

    public:
        descriptor_pool_t& operator=(descriptor_pool_t const&) = delete;

        descriptor_pool_t& operator=(descriptor_pool_t&& other) noexcept;

    private:
        device_t* device_{};
        VkDescriptorPool pool_{VK_NULL_HANDLE};
    };

    [[nodiscard]] std::expected<descriptor_pool_t, VkResult>
    create_descriptor_pool(device_t& device,
        std::span<VkDescriptorPoolSize const> const& pool_sizes,
        uint32_t max_sets,
        VkDescriptorPoolCreateFlags flags =
            VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);
} // namespace vkrndr

#endif
