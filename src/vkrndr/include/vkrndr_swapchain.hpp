#ifndef VKRNDR_SWAPCHAIN_INCLUDED
#define VKRNDR_SWAPCHAIN_INCLUDED

#include <volk.h>

#include <vkrndr_synchronization.hpp>
#include <vkrndr_utility.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace vkrndr
{
    struct device_t;
    class execution_port_t;
    class window_t;
} // namespace vkrndr

namespace vkrndr
{
    namespace detail
    {
        struct [[nodiscard]] swap_frame_t final
        {
            VkSemaphore image_available{VK_NULL_HANDLE};
            VkFence in_flight{VK_NULL_HANDLE};
        };

        struct [[nodiscard]] swap_image_t final
        {
            VkImage handle;
            VkImageView view;
        };
    } // namespace detail

    struct [[nodiscard]] swapchain_settings_t final
    {
        VkFormat preferred_format{VK_FORMAT_B8G8R8A8_SRGB};
        VkImageUsageFlags image_flags{VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT};
        VkPresentModeKHR preferred_present_mode{VK_PRESENT_MODE_MAILBOX_KHR};
        uint32_t frames_in_flight{2};
        execution_port_t* present_queue{nullptr};
    };

    class [[nodiscard]] swapchain_t final
    {
    public: // Construction
        swapchain_t(window_t const& window,
            device_t& device,
            swapchain_settings_t const& settings);

        swapchain_t(swapchain_t const&) = delete;

        swapchain_t(swapchain_t&& other) noexcept = delete;

    public: // Destruction
        ~swapchain_t();

    public: // Interface
        [[nodiscard]] VkExtent2D extent() const noexcept;

        [[nodiscard]] VkSwapchainKHR swapchain() const noexcept;

        [[nodiscard]] VkFormat image_format() const noexcept;

        [[nodiscard]] uint32_t min_image_count() const noexcept;

        [[nodiscard]] uint32_t image_count() const noexcept;

        [[nodiscard]] VkImage image(uint32_t image_index) const noexcept;

        [[nodiscard]] VkImageView image_view(
            uint32_t image_index) const noexcept;

        [[nodiscard]] bool needs_refresh() const noexcept;

        [[nodiscard]] VkPresentModeKHR present_mode() const noexcept;

        [[nodiscard]] std::span<VkPresentModeKHR const>
        available_present_modes() const noexcept;

        [[nodiscard]] bool acquire_next_image(size_t current_frame,
            uint32_t& image_index);

        void submit_command_buffers(
            std::span<VkCommandBuffer const> command_buffers,
            size_t current_frame,
            uint32_t image_index);

        [[nodiscard]] bool change_present_mode(VkPresentModeKHR new_mode);

        void recreate();

    public: // Operators
        swapchain_t& operator=(swapchain_t const&) = delete;

        swapchain_t& operator=(swapchain_t&& other) noexcept = delete;

    private: // Helpers
        void create_swap_frames(bool is_recreated);

        void cleanup();

    private:
        window_t const* window_{};
        device_t* device_{};
        swapchain_settings_t settings_{};
        bool swapchain_maintenance_1_enabled_{};

        fence_pool_t fence_pool_;
        semaphore_pool_t semaphore_pool_;

        VkFormat image_format_{};
        uint32_t min_image_count_{};
        VkExtent2D extent_{};

        VkSwapchainKHR handle_{};
        std::vector<detail::swap_frame_t> frames_;
        std::vector<VkSemaphore> submit_finished_semaphore_;
        std::vector<detail::swap_image_t> images_;

        VkPresentModeKHR current_present_mode_{VK_PRESENT_MODE_FIFO_KHR};
        VkPresentModeKHR desired_present_mode_{VK_PRESENT_MODE_FIFO_KHR};
        std::vector<VkPresentModeKHR> available_present_modes_;
        std::vector<VkPresentModeKHR> compatible_present_modes_;

        bool swapchain_refresh_needed_{};
    };
} // namespace vkrndr

inline VkExtent2D vkrndr::swapchain_t::extent() const noexcept
{
    return extent_;
}

inline VkSwapchainKHR vkrndr::swapchain_t::swapchain() const noexcept
{
    return handle_;
}

inline VkFormat vkrndr::swapchain_t::image_format() const noexcept
{
    return image_format_;
}

inline uint32_t vkrndr::swapchain_t::min_image_count() const noexcept
{
    return min_image_count_;
}

inline uint32_t vkrndr::swapchain_t::image_count() const noexcept
{
    return vkrndr::count_cast(images_.size());
}

inline VkImage vkrndr::swapchain_t::image(
    uint32_t const image_index) const noexcept
{
    return images_[image_index].handle;
}

inline VkImageView vkrndr::swapchain_t::image_view(
    uint32_t const image_index) const noexcept
{
    return images_[image_index].view;
}

inline bool vkrndr::swapchain_t::needs_refresh() const noexcept
{
    return swapchain_refresh_needed_;
}

inline VkPresentModeKHR vkrndr::swapchain_t::present_mode() const noexcept
{
    return current_present_mode_;
}

inline std::span<VkPresentModeKHR const>
vkrndr::swapchain_t::available_present_modes() const noexcept
{
    return available_present_modes_;
}

#endif
