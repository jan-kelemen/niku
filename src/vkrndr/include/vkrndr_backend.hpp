#ifndef VKRNDR_BACKEND_INCLUDED
#define VKRNDR_BACKEND_INCLUDED

#include <vkrndr_command_pool.hpp>
#include <vkrndr_context.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_render_settings.hpp>

#include <cppext_cycled_buffer.hpp>

#include <vulkan/vulkan_core.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <variant>
#include <vector>

namespace vkrndr
{
    struct buffer_t;
    class scene_t;
    class swap_chain_t;
    class window_t;
    class execution_port_t;
} // namespace vkrndr

namespace vkrndr
{
    using swapchain_acquire_t =
        std::variant<std::monostate, image_t, VkExtent2D>;

    class [[nodiscard]] backend_t final
    {
    public: // Construction
        backend_t(window_t* window,
            render_settings_t const& settings,
            bool debug);

        backend_t(backend_t const&) = delete;

        backend_t(backend_t&&) noexcept = delete;

    public: // Destruction
        ~backend_t();

    public: // Interface
        [[nodiscard]] constexpr VkDescriptorPool
        descriptor_pool() const noexcept;

        [[nodiscard]] constexpr device_t& device() noexcept;

        [[nodiscard]] constexpr device_t const& device() const noexcept;

        [[nodiscard]] VkFormat image_format() const;

        [[nodiscard]] uint32_t image_count() const;

        [[nodiscard]] VkExtent2D extent() const;

        [[nodiscard]] swapchain_acquire_t begin_frame();

        void end_frame();

        [[nodiscard]] VkCommandBuffer request_command_buffer(
            bool transfer_only);

        void draw(scene_t& scene, image_t const& target_image);

        [[nodiscard]] image_t load_texture(
            std::filesystem::path const& texture_path,
            VkFormat format);

        [[nodiscard]] image_t transfer_image(
            std::span<std::byte const> image_data,
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
            std::unique_ptr<command_pool_t> present_command_pool{};
            std::vector<VkCommandBuffer> present_command_buffers;
            size_t used_present_command_buffers{};

            execution_port_t* transfer_queue{};
            std::unique_ptr<command_pool_t> transfer_command_pool{};
            std::vector<VkCommandBuffer> transfer_command_buffers;
            size_t used_transfer_command_buffers{};
        };

    private: // Data
        render_settings_t render_settings_;

        window_t* window_;
        context_t context_;
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

[[nodiscard]] constexpr vkrndr::device_t& vkrndr::backend_t::device() noexcept
{
    return device_;
}

[[nodiscard]] constexpr vkrndr::device_t const&
vkrndr::backend_t::device() const noexcept
{
    return device_;
}

#endif
