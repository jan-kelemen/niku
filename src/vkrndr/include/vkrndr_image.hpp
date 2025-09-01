#ifndef VKRNDR_IMAGE_INCLUDED
#define VKRNDR_IMAGE_INCLUDED

#include <vkrndr_synchronization.hpp>

#include <vma_impl.hpp>

#include <volk.h>

#include <cstdint>
#include <span>
#include <string_view>

namespace vkrndr
{
    struct device_t;
} // namespace vkrndr

namespace vkrndr
{
    struct [[nodiscard]] image_t final
    {
        VkImage handle{VK_NULL_HANDLE};
        VmaAllocation allocation{VK_NULL_HANDLE};
        VkImageView view{VK_NULL_HANDLE};
        VkFormat format{};
        VkSampleCountFlags sample_count{};
        uint32_t mip_levels{};
        VkExtent3D extent{};

        [[nodiscard]] constexpr operator VkImage() const noexcept;
    };

    void destroy(device_t const& device, image_t const& image);

    struct [[nodiscard]] image_create_info_t final
    {
        void const* chain{};
        VkImageCreateFlags image_flags{};
        VkImageType type{};
        VkFormat format{};
        VkExtent3D extent{};
        uint32_t mip_levels{1};
        uint32_t array_layers{1};
        VkSampleCountFlagBits samples{VK_SAMPLE_COUNT_1_BIT};
        VkImageTiling tiling{};
        VkImageUsageFlags usage{};
        std::span<uint32_t const> sharing_queue_families;
        VkImageLayout initial_layout{VK_IMAGE_LAYOUT_UNDEFINED};
        VmaAllocationCreateFlags allocation_flags{};
        VkMemoryPropertyFlags required_memory_flags{};
        VkMemoryPropertyFlags preferred_memory_flags{};
        uint32_t memory_type_bits{};
        VmaPool pool{VK_NULL_HANDLE};
        float priority{};
    };

    struct [[nodiscard]] image_1d_create_info_t final
    {
        void const* chain{};
        VkImageCreateFlags image_flags{};
        VkFormat format{};
        uint32_t extent{};
        uint32_t mip_levels{1};
        uint32_t array_layers{1};
        VkImageTiling tiling{};
        VkImageUsageFlags usage{};
        std::span<uint32_t const> sharing_queue_families;
        VkImageLayout initial_layout{VK_IMAGE_LAYOUT_UNDEFINED};
        VmaAllocationCreateFlags allocation_flags{};
        VkMemoryPropertyFlags required_memory_flags{};
        VkMemoryPropertyFlags preferred_memory_flags{};
        uint32_t memory_type_bits{};
        VmaPool pool{VK_NULL_HANDLE};
        float priority{};

        constexpr operator image_create_info_t()
        {
            return {.chain = chain,
                .type = VK_IMAGE_TYPE_1D,
                .format = format,
                .extent = {extent, 1, 1},
                .mip_levels = mip_levels,
                .array_layers = array_layers,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .tiling = tiling,
                .usage = usage,
                .sharing_queue_families = sharing_queue_families,
                .initial_layout = initial_layout,
                .allocation_flags = allocation_flags,
                .required_memory_flags = required_memory_flags,
                .preferred_memory_flags = preferred_memory_flags,
                .memory_type_bits = memory_type_bits,
                .pool = pool,
                .priority = priority};
        }
    };

    struct [[nodiscard]] image_2d_create_info_t final
    {
        void const* chain{};
        VkImageCreateFlags image_flags{};
        VkFormat format{};
        VkExtent2D extent{};
        uint32_t mip_levels{1};
        uint32_t array_layers{1};
        VkSampleCountFlagBits samples{VK_SAMPLE_COUNT_1_BIT};
        VkImageTiling tiling{};
        VkImageUsageFlags usage{};
        std::span<uint32_t const> sharing_queue_families;
        VkImageLayout initial_layout{VK_IMAGE_LAYOUT_UNDEFINED};
        VmaAllocationCreateFlags allocation_flags{};
        VkMemoryPropertyFlags required_memory_flags{};
        VkMemoryPropertyFlags preferred_memory_flags{};
        uint32_t memory_type_bits{};
        VmaPool pool{VK_NULL_HANDLE};
        float priority{};

        constexpr operator image_create_info_t()
        {
            return {.chain = chain,
                .type = VK_IMAGE_TYPE_2D,
                .format = format,
                .extent = {extent.width, extent.height, 1},
                .mip_levels = mip_levels,
                .array_layers = array_layers,
                .samples = samples,
                .tiling = tiling,
                .usage = usage,
                .sharing_queue_families = sharing_queue_families,
                .initial_layout = initial_layout,
                .allocation_flags = allocation_flags,
                .required_memory_flags = required_memory_flags,
                .preferred_memory_flags = preferred_memory_flags,
                .memory_type_bits = memory_type_bits,
                .pool = pool,
                .priority = priority};
        }
    };

    struct [[nodiscard]] image_3d_create_info_t final
    {
        void const* chain{};
        VkImageCreateFlags image_flags{};
        VkFormat format{};
        VkExtent3D extent{};
        uint32_t mip_levels{1};
        VkImageTiling tiling{};
        VkImageUsageFlags usage{};
        std::span<uint32_t const> sharing_queue_families;
        VkImageLayout initial_layout{VK_IMAGE_LAYOUT_UNDEFINED};
        VmaAllocationCreateFlags allocation_flags{};
        VkMemoryPropertyFlags required_memory_flags{};
        VkMemoryPropertyFlags preferred_memory_flags{};
        uint32_t memory_type_bits{};
        VmaPool pool{VK_NULL_HANDLE};
        float priority{};

        constexpr operator image_create_info_t()
        {
            return {.chain = chain,
                .type = VK_IMAGE_TYPE_1D,
                .format = format,
                .extent = {extent.width, extent.height, 1},
                .mip_levels = mip_levels,
                .array_layers = 1,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .tiling = tiling,
                .usage = usage,
                .sharing_queue_families = sharing_queue_families,
                .initial_layout = initial_layout,
                .allocation_flags = allocation_flags,
                .required_memory_flags = required_memory_flags,
                .preferred_memory_flags = preferred_memory_flags,
                .memory_type_bits = memory_type_bits,
                .pool = pool,
                .priority = priority};
        }
    };

    image_t create_image(device_t const& device,
        image_create_info_t const& create_info);

    [[nodiscard]] VkImageView create_image_view(device_t const& device,
        VkImage image,
        VkFormat format,
        VkImageAspectFlags aspect_flags,
        uint32_t mip_levels,
        uint32_t base_mip_level = 0);

    image_t create_image_and_view(device_t const& device,
        image_create_info_t const& create_info,
        VkImageAspectFlags aspect_flags);

    [[nodiscard]] constexpr VkImageMemoryBarrier2 image_barrier(
        vkrndr::image_t const& image,
        VkImageAspectFlags const aspect = VK_IMAGE_ASPECT_COLOR_BIT)
    {
        return image_barrier(image,
            whole_resource(aspect, image.mip_levels, 1));
    }

    void object_name(device_t const& device,
        image_t const& image,
        std::string_view name);
} // namespace vkrndr

inline constexpr vkrndr::image_t::operator VkImage() const noexcept
{
    return handle;
}
#endif
