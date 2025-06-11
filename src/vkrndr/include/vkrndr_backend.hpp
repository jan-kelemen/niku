#ifndef VKRNDR_BACKEND_INCLUDED
#define VKRNDR_BACKEND_INCLUDED

#include <vkrndr_command_pool.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_instance.hpp>
#include <vkrndr_render_settings.hpp>
#include <vkrndr_transient_operation.hpp>

#include <cppext_cycled_buffer.hpp>

#include <volk.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <variant>
#include <vector>

namespace vkrndr
{
    struct buffer_t;
    class execution_port_t;
    struct library_handle_t;
    class swap_chain_t;
    class window_t;
} // namespace vkrndr

namespace vkrndr
{
    using swapchain_acquire_t = std::variant<bool, VkExtent2D>;

    class [[nodiscard]] backend_t final
    {
    public: // Construction
        backend_t(std::shared_ptr<library_handle_t>&& library,
            window_t& window,
            render_settings_t const& settings,
            bool debug);

        backend_t(backend_t const&) = delete;

        backend_t(backend_t&&) noexcept = delete;

    public: // Destruction
        ~backend_t();

    public: // Interface
        [[nodiscard]] constexpr VkDescriptorPool
        descriptor_pool() const noexcept;

        [[nodiscard]] constexpr instance_t& instance() noexcept;

        [[nodiscard]] constexpr instance_t const& instance() const noexcept;

        [[nodiscard]] constexpr device_t& device() noexcept;

        [[nodiscard]] constexpr device_t const& device() const noexcept;

        [[nodiscard]] constexpr swap_chain_t& swap_chain() noexcept;

        [[nodiscard]] constexpr swap_chain_t const& swap_chain() const noexcept;

        [[nodiscard]] VkFormat image_format() const;

        [[nodiscard]] uint32_t image_count() const;

        [[nodiscard]] uint32_t frames_in_flight() const;

        [[nodiscard]] VkExtent2D extent() const;

        [[nodiscard]] image_t swapchain_image();

        [[nodiscard]] swapchain_acquire_t begin_frame();

        void end_frame();

        [[nodiscard]] VkCommandBuffer request_command_buffer();

        [[nodiscard]] transient_operation_t request_transient_operation(
            bool transfer_only);

        void draw();

        [[nodiscard]] image_t transfer_image(
            std::span<std::byte const> const& image_data,
            VkExtent2D extent,
            VkFormat format,
            uint32_t mip_levels);

        [[nodiscard]] image_t transfer_buffer_to_image(
            vkrndr::buffer_t const& source,
            VkExtent2D extent,
            VkFormat format,
            uint32_t mip_levels);

        void transfer_buffer(buffer_t const& source, buffer_t const& target);

    public: // Operators
        backend_t& operator=(backend_t const&) = delete;

        backend_t& operator=(backend_t&&) noexcept = delete;

    private: // Types
        struct [[nodiscard]] frame_data_t final
        {
            execution_port_t* present_queue{};
            std::unique_ptr<command_pool_t> present_command_pool;
            std::unique_ptr<command_pool_t> present_transient_command_pool;
            std::vector<VkCommandBuffer> present_command_buffers;
            size_t used_present_command_buffers{};

            execution_port_t* transfer_queue{};
            std::unique_ptr<command_pool_t> transfer_transient_command_pool;
        };

    private: // Data
        std::shared_ptr<library_handle_t> library_;

        render_settings_t render_settings_;

        window_t* window_;
        instance_t instance_;
        device_t device_;
        std::unique_ptr<swap_chain_t> swap_chain_;

        cppext::cycled_buffer_t<frame_data_t> frame_data_;

        VkDescriptorPool descriptor_pool_{};

        uint32_t image_index_{};
    };
} // namespace vkrndr

constexpr VkDescriptorPool vkrndr::backend_t::descriptor_pool() const noexcept
{
    return descriptor_pool_;
}

constexpr vkrndr::instance_t& vkrndr::backend_t::instance() noexcept
{
    return instance_;
}

constexpr vkrndr::instance_t const& vkrndr::backend_t::instance() const noexcept
{
    return instance_;
}

constexpr vkrndr::device_t& vkrndr::backend_t::device() noexcept
{
    return device_;
}

constexpr vkrndr::device_t const& vkrndr::backend_t::device() const noexcept
{
    return device_;
}

constexpr vkrndr::swap_chain_t& vkrndr::backend_t::swap_chain() noexcept
{
    return *swap_chain_;
}

constexpr vkrndr::swap_chain_t const&
vkrndr::backend_t::swap_chain() const noexcept
{
    return *swap_chain_;
}

#endif
