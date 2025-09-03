#ifndef VKRNDR_SWAPCHAIN_INCLUDED
#define VKRNDR_SWAPCHAIN_INCLUDED

#include <vkrndr_image.hpp>
#include <vkrndr_synchronization.hpp>
#include <vkrndr_utility.hpp>

#include <boost/container/deque.hpp>
#include <boost/container/small_vector.hpp>

#include <volk.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

// IWYU pragma: no_include <boost/intrusive/detail/iterator.hpp>
// IWYU pragma: no_include <boost/move/detail/addressof.hpp>
// IWYU pragma: no_include <boost/move/utility_core.hpp>

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
        inline constexpr uint32_t invalid_image_index{
            std::numeric_limits<uint32_t>::max()};

        struct [[nodiscard]] swap_image_t final
        {
            VkImage handle;
            VkImageView view;
        };

        struct [[nodiscard]] swap_frame_t final
        {
            VkSemaphore acquire_semaphore{VK_NULL_HANDLE};
            VkSemaphore present_semaphore{VK_NULL_HANDLE};

            uint32_t image_index{};

            std::vector<swap_image_t> garbage;
        };

        struct [[nodiscard]] cleanup_data_t final
        {
            VkSwapchainKHR swapchain;
            std::vector<VkSemaphore> semaphores;
        };

        struct [[nodiscard]] present_operation_t final
        {
            VkFence cleanup_fence{VK_NULL_HANDLE};
            VkSemaphore present_semaphore{VK_NULL_HANDLE};
            boost::container::small_vector<cleanup_data_t, 1> old_swapchains;
            uint32_t image_index{invalid_image_index};
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

        [[nodiscard]] bool needs_refresh() const noexcept;

        [[nodiscard]] VkPresentModeKHR present_mode() const noexcept;

        [[nodiscard]] std::span<VkPresentModeKHR const>
        available_present_modes() const noexcept;

        [[nodiscard]] std::optional<vkrndr::image_t> acquire_next_image(
            size_t current_frame);

        void submit_command_buffers(
            std::span<VkCommandBuffer const> command_buffers,
            size_t current_frame,
            VkFence submit_fence);

        [[nodiscard]] bool change_present_mode(VkPresentModeKHR new_mode);

        void recreate(uint32_t current_frame);

    public: // Operators
        swapchain_t& operator=(swapchain_t const&) = delete;

        swapchain_t& operator=(swapchain_t&& other) noexcept = delete;

    private: // Helpers
        void create_swap_frames(bool is_recreated, uint32_t current_frame);

        void cleanup_images(std::vector<detail::swap_image_t>& images);

        void schedule_destruction(VkSwapchainKHR swapchain);

        void associate_with_present_history(uint32_t image_index,
            VkFence fence);

        void add_to_present_history(uint32_t image_index,
            VkFence present_fence,
            detail::swap_frame_t& frame);

        void cleanup_present_history();

        void cleanup_present_info(detail::present_operation_t& operation);

        void cleanup_swapchain(detail::cleanup_data_t& data);

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

        VkPresentModeKHR current_present_mode_{VK_PRESENT_MODE_FIFO_KHR};
        VkPresentModeKHR desired_present_mode_{VK_PRESENT_MODE_FIFO_KHR};
        std::vector<VkPresentModeKHR> available_present_modes_;
        std::vector<VkPresentModeKHR> compatible_present_modes_;

        VkSwapchainKHR handle_{VK_NULL_HANDLE};
        std::vector<detail::swap_frame_t> frames_in_flight_;
        std::vector<detail::swap_image_t> images_;
        boost::container::deque<detail::present_operation_t> present_history_;
        boost::container::small_vector<detail::cleanup_data_t, 1>
            old_swapchains_;

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
    return count_cast(images_);
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
