#ifndef VKRNDR_BACKEND_INCLUDED
#define VKRNDR_BACKEND_INCLUDED

#include <vkrndr_device.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_instance.hpp>
#include <vkrndr_library_handle.hpp>
#include <vkrndr_transient_operation.hpp>

#include <cppext_cycled_buffer.hpp>

#include <volk.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <vector>

namespace vkrndr
{
    struct buffer_t;
    class execution_port_t;
} // namespace vkrndr

namespace vkrndr
{
    struct rendering_context_t final
    {
        library_handle_ptr_t library_handle;
        instance_ptr_t instance;
        device_ptr_t device;
    };

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
        [[nodiscard]] instance_t& instance() noexcept;

        [[nodiscard]] instance_t const& instance() const noexcept;

        [[nodiscard]] device_t& device() noexcept;

        [[nodiscard]] device_t const& device() const noexcept;

        [[nodiscard]] VkDescriptorPool descriptor_pool();

        [[nodiscard]] uint32_t frames_in_flight() const;

        void begin_frame();

        [[nodiscard]] std::span<VkCommandBuffer const> present_buffers();

        void end_frame();

        [[nodiscard]] VkCommandBuffer request_command_buffer();

        [[nodiscard]] transient_operation_t request_transient_operation(
            bool transfer_only);

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

#endif
