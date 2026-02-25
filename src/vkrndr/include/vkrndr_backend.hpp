#ifndef VKRNDR_BACKEND_INCLUDED
#define VKRNDR_BACKEND_INCLUDED

#include <vkrndr_commands.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_rendering_context.hpp>

#include <cppext_container.hpp>
#include <cppext_cycled_buffer.hpp>

#include <volk.h>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <span>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

// IWYU pragma: no_include <boost/smart_ptr/intrusive_ref_counter.hpp>

namespace vkrndr
{
    struct buffer_t;
    struct device_t;
    class execution_port_t;
} // namespace vkrndr

namespace vkrndr
{
    class [[nodiscard]] backend_t final
    {
    public: // Construction
        backend_t(rendering_context_t rendering_context,
            uint32_t frames_in_flight);

        backend_t(backend_t const&) = delete;

        backend_t(backend_t&&) noexcept = delete;

    public: // Destruction
        ~backend_t();

    public: // Interface
        [[nodiscard]] device_t const& device() const noexcept;

        [[nodiscard]] VkDescriptorPool descriptor_pool();

        [[nodiscard]] uint32_t frames_in_flight() const;

        void begin_frame();

        [[nodiscard]] std::span<VkCommandBuffer const> present_buffers();

        void end_frame();

        [[nodiscard]] VkCommandBuffer request_command_buffer();

        template<typename Func>
        std::expected<std::invoke_result_t<Func, VkCommandBuffer>,
            std::error_code>
        execute_immediate(bool transfer_only, Func&& callback)
        requires(std::invocable<Func, VkCommandBuffer>);

        [[nodiscard]] image_t transfer_image(
            std::span<std::byte const> const& image_data,
            VkExtent2D extent,
            VkFormat format,
            uint32_t mip_levels,
            std::span<image_mip_level_t> const& defined_mips = {});

        [[nodiscard]] image_t transfer_buffer_to_image(
            vkrndr::buffer_t const& source,
            VkExtent2D extent,
            VkFormat format,
            uint32_t mip_levels,
            std::span<image_mip_level_t> const& defined_mips = {});

        void transfer_buffer(buffer_t const& source, buffer_t const& target);

    public: // Operators
        backend_t& operator=(backend_t const&) = delete;

        backend_t& operator=(backend_t&&) noexcept = delete;

    private: // Types
        struct [[nodiscard]] frame_data_t final
        {
            execution_port_t* present_queue{};
            VkCommandPool present_command_pool{VK_NULL_HANDLE};
            VkCommandPool present_transient_command_pool{VK_NULL_HANDLE};
            std::vector<VkCommandBuffer> present_command_buffers;
            size_t used_present_command_buffers{};

            execution_port_t* transfer_queue{};
            VkCommandPool transfer_transient_command_pool{VK_NULL_HANDLE};
        };

    private: // Data
        rendering_context_t context_;

        VkDescriptorPool descriptor_pool_;

        cppext::cycled_buffer_t<frame_data_t> frame_data_;
    };
} // namespace vkrndr

template<typename Func>
std::expected<std::invoke_result_t<Func, VkCommandBuffer>, std::error_code>
vkrndr::backend_t::execute_immediate(bool const transfer_only, Func&& callback)
requires(std::invocable<Func, VkCommandBuffer>)
{
    using R = std::invoke_result_t<Func, VkCommandBuffer>;

    VkCommandPool const pool{transfer_only
            ? frame_data_->transfer_transient_command_pool
            : frame_data_->present_transient_command_pool};

    // This is a fake initial state, to avoid issues with types that don't have
    // a default constructor
    std::expected<R, std::error_code> rv{
        std::unexpected{make_error_code(std::errc::no_message)}};

    VkCommandBuffer cb{VK_NULL_HANDLE};
    if (std::expected<void, std::error_code> const result{
            begin_single_time_commands(*context_.device,
                pool,
                cppext::as_span(cb))};
        !result)
    {
        rv = std::unexpected{result.error()};
    }

    if constexpr (std::is_void_v<R>)
    {
        std::invoke(std::forward<Func>(callback), cb);
        rv = {};
    }
    else
    {
        rv = std::invoke(std::forward<Func>(callback), cb);
    }

    execution_port_t* const queue{transfer_only ? frame_data_->transfer_queue
                                                : frame_data_->present_queue};
    if (std::expected<void, std::error_code> const result{
            end_single_time_commands(*context_.device,
                pool,
                *queue,
                cppext::as_span(cb))};
        !result)
    {
        rv = std::unexpected{result.error()};
    }

    return rv;
}

#endif
