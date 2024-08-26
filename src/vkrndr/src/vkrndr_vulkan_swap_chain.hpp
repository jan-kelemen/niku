#ifndef VKRNDR_VULKAN_SWAP_CHAIN_INCLUDED
#define VKRNDR_VULKAN_SWAP_CHAIN_INCLUDED

#include <vulkan/vulkan_core.h>

#include <vkrndr_vulkan_utility.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace vkrndr
{
    struct render_settings;
    struct vulkan_context;
    struct vulkan_device;
    struct vulkan_queue;
    class vulkan_window;
} // namespace vkrndr

namespace vkrndr
{
    namespace detail
    {
        struct [[nodiscard]] swap_frame final
        {
            VkImage image;
            VkImageView image_view;
            VkSemaphore image_available;
            VkSemaphore render_finished;
            VkFence in_flight;
        };

        void destroy(vulkan_device const* device, swap_frame* frame);
    } // namespace detail

    struct [[nodiscard]] swap_chain_support final
    {
        VkSurfaceCapabilitiesKHR capabilities{};
        std::vector<VkSurfaceFormatKHR> surface_formats;
        std::vector<VkPresentModeKHR> present_modes;
    };

    swap_chain_support query_swap_chain_support(VkPhysicalDevice device,
        VkSurfaceKHR surface);

    class [[nodiscard]] vulkan_swap_chain final
    {
    public: // Constants
        static constexpr int max_frames_in_flight{2};

    public: // Construction
        vulkan_swap_chain(vulkan_window* window,
            vulkan_context* context,
            vulkan_device* device,
            render_settings const* settings);

        vulkan_swap_chain(vulkan_swap_chain const&) = delete;

        vulkan_swap_chain(vulkan_swap_chain&& other) noexcept = delete;

    public: // Destruction
        ~vulkan_swap_chain();

    public: // Interface
        [[nodiscard]] constexpr VkExtent2D extent() const noexcept;

        [[nodiscard]] constexpr VkSwapchainKHR swap_chain() const noexcept;

        [[nodiscard]] constexpr VkFormat image_format() const noexcept;

        [[nodiscard]] constexpr uint32_t min_image_count() const noexcept;

        [[nodiscard]] constexpr uint32_t image_count() const noexcept;

        [[nodiscard]] constexpr VkImage image(
            uint32_t image_index) const noexcept;

        [[nodiscard]] constexpr VkImageView image_view(
            uint32_t image_index) const noexcept;

        [[nodiscard]] bool acquire_next_image(size_t current_frame,
            uint32_t& image_index);

        void submit_command_buffers(
            std::span<VkCommandBuffer const> command_buffers,
            size_t current_frame,
            uint32_t image_index);

        void recreate();

    public: // Operators
        vulkan_swap_chain& operator=(vulkan_swap_chain const&) = delete;

        vulkan_swap_chain& operator=(
            vulkan_swap_chain&& other) noexcept = delete;

    private: // Helpers
        void create_swap_frames();

        void cleanup();

    private:
        vulkan_window* window_{};
        vulkan_context* context_{};
        vulkan_device* device_{};
        render_settings const* settings_{};
        vulkan_queue* present_queue_{};
        VkFormat image_format_{};
        uint32_t min_image_count_{};
        VkExtent2D extent_{};
        VkSwapchainKHR chain_{};
        std::vector<detail::swap_frame> frames_;
    };

} // namespace vkrndr

constexpr VkExtent2D vkrndr::vulkan_swap_chain::extent() const noexcept
{
    return extent_;
}

constexpr VkSwapchainKHR vkrndr::vulkan_swap_chain::swap_chain() const noexcept
{
    return chain_;
}

constexpr VkFormat vkrndr::vulkan_swap_chain::image_format() const noexcept
{
    return image_format_;
}

constexpr uint32_t vkrndr::vulkan_swap_chain::min_image_count() const noexcept
{
    return min_image_count_;
}

constexpr uint32_t vkrndr::vulkan_swap_chain::image_count() const noexcept
{
    return vkrndr::count_cast(frames_.size());
}

constexpr VkImage vkrndr::vulkan_swap_chain::image(
    uint32_t const image_index) const noexcept
{
    return frames_[image_index].image;
}

constexpr VkImageView vkrndr::vulkan_swap_chain::image_view(
    uint32_t const image_index) const noexcept
{
    return frames_[image_index].image_view;
}

#endif // !VKRNDR_VULKAN_SWAP_CHAIN_INCLUDED
