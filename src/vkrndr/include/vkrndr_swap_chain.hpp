#ifndef VKRNDR_SWAP_CHAIN_INCLUDED
#define VKRNDR_SWAP_CHAIN_INCLUDED

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

    struct [[nodiscard]] swap_chain_support_t final
    {
        VkSurfaceCapabilitiesKHR capabilities{};
        std::vector<VkSurfaceFormatKHR> surface_formats;
        std::vector<VkPresentModeKHR> present_modes;
    };

    swap_chain_support_t query_swap_chain_support(VkPhysicalDevice device,
        VkSurfaceKHR surface);

    class [[nodiscard]] swap_chain_t final
    {
    public: // Construction
        swap_chain_t(window_t const& window,
            device_t& device,
            render_settings_t const& settings);

        swap_chain_t(swap_chain_t const&) = delete;

        swap_chain_t(swap_chain_t&& other) noexcept = delete;

    public: // Destruction
        ~swap_chain_t();

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

        [[nodiscard]] constexpr VkPresentModeKHR present_mode() const;

        [[nodiscard]] std::span<VkPresentModeKHR const>
        available_present_modes() const;

        [[nodiscard]] bool change_present_mode(VkPresentModeKHR new_mode);

        void recreate();

    public: // Operators
        swap_chain_t& operator=(swap_chain_t const&) = delete;

        swap_chain_t& operator=(swap_chain_t&& other) noexcept = delete;

    private: // Helpers
        void create_swap_frames(bool is_recreated);

        void cleanup();

    private:
        window_t const* window_{};
        device_t* device_{};
        render_settings_t const* settings_{};
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

constexpr VkExtent2D vkrndr::swap_chain_t::extent() const noexcept
{
    return extent_;
}

constexpr VkSwapchainKHR vkrndr::swap_chain_t::swap_chain() const noexcept
{
    return chain_;
}

constexpr VkFormat vkrndr::swap_chain_t::image_format() const noexcept
{
    return image_format_;
}

constexpr uint32_t vkrndr::swap_chain_t::min_image_count() const noexcept
{
    return min_image_count_;
}

constexpr uint32_t vkrndr::swap_chain_t::image_count() const noexcept
{
    return vkrndr::count_cast(frames_.size());
}

constexpr VkImage vkrndr::swap_chain_t::image(
    uint32_t const image_index) const noexcept
{
    return images_[image_index].image;
}

constexpr VkImageView vkrndr::swap_chain_t::image_view(
    uint32_t const image_index) const noexcept
{
    return images_[image_index].view;
}

constexpr VkPresentModeKHR vkrndr::swap_chain_t::present_mode() const
{
    return current_present_mode_;
}

#endif // !VKRNDR_VULKAN_SWAP_CHAIN_INCLUDED
