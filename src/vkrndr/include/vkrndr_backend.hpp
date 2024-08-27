#ifndef VKRNDR_BACKEND_INCLUDED
#define VKRNDR_BACKEND_INCLUDED

#include <vkrndr_context.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_font.hpp>
#include <vkrndr_gltf_manager.hpp>
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
    class font_manager_t;
    class imgui_render_layer_t;
    struct buffer_t;
    class scene_t;
    class swap_chain_t;
    class window_t;
    struct queue_t;
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

        [[nodiscard]] bool imgui_layer() const;

        void imgui_layer(bool state);

        [[nodiscard]] swapchain_acquire_t begin_frame();

        void end_frame();

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

        [[nodiscard]] font_t load_font(std::filesystem::path const& font_path,
            uint32_t font_size);

        [[nodiscard]] std::unique_ptr<vkrndr::gltf_model_t> load_model(
            std::filesystem::path const& model_path);

    public: // Operators
        backend_t& operator=(backend_t const&) = delete;

        backend_t& operator=(backend_t&&) noexcept = delete;

    private: // Types
        struct [[nodiscard]] frame_data_t final
        {
            queue_t* present_queue{};
            VkCommandPool present_command_pool{VK_NULL_HANDLE};
            std::vector<VkCommandBuffer> present_command_buffers;
            size_t used_present_command_buffers_{};

            queue_t* transfer_queue{};
            VkCommandPool transfer_command_pool{VK_NULL_HANDLE};
            std::vector<VkCommandBuffer> transfer_command_buffers;
            size_t used_transfer_command_buffers{};
        };

    private:
        [[nodiscard]] VkCommandBuffer request_command_buffer(
            bool transfer_only);

    private: // Data
        render_settings_t render_settings_;

        window_t* window_;
        context_t context_;
        device_t device_;

        std::unique_ptr<swap_chain_t> swap_chain_;

        cppext::cycled_buffer_t<frame_data_t> frame_data_;

        VkDescriptorPool descriptor_pool_{};

        bool imgui_layer_enabled_;
        std::unique_ptr<imgui_render_layer_t> imgui_layer_;
        std::unique_ptr<font_manager_t> font_manager_;
        std::unique_ptr<gltf_manager_t> gltf_manager_;

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
