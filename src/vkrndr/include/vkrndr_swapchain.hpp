#ifndef VKRNDR_SWAPCHAIN_INCLUDED
#define VKRNDR_SWAPCHAIN_INCLUDED

#include <volk.h>

#include <vkrndr_image.hpp>
#include <vkrndr_utility.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace vkrndr
{
    struct render_settings_t;
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

        void destroy(device_t const* device, swap_frame_t* frame);
    } // namespace detail

    class [[nodiscard]] swapchain_t final
    {
    public: // Construction
        swapchain_t(window_t const& window,
            device_t& device,
            render_settings_t const& settings);

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

        [[nodiscard]] bool acquire_next_image(size_t current_frame,
            uint32_t& image_index);

        void submit_command_buffers(
            std::span<VkCommandBuffer const> command_buffers,
            size_t current_frame,
            uint32_t image_index);

        [[nodiscard]] VkPresentModeKHR present_mode() const;

        [[nodiscard]] std::span<VkPresentModeKHR const>
        available_present_modes() const;

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
        render_settings_t const* settings_{};
        bool swapchain_maintenance_1_enabled_{};
        bool swapchain_refresh_needed_{};
        execution_port_t* present_queue_{};
        VkFormat image_format_{};
        uint32_t min_image_count_{};
        VkExtent2D extent_{};
        VkSwapchainKHR chain_{};
        VkPresentModeKHR current_present_mode_{VK_PRESENT_MODE_FIFO_KHR};
        VkPresentModeKHR desired_present_mode_{VK_PRESENT_MODE_FIFO_KHR};
        std::vector<VkPresentModeKHR> available_present_modes_;
        std::vector<VkPresentModeKHR> compatible_present_modes_;
        std::vector<detail::swap_frame_t> frames_;
        std::vector<VkSemaphore> submit_finished_semaphore_;
        std::vector<vkrndr::image_t> images_;
    };
} // namespace vkrndr

#endif
