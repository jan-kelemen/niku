#ifndef VKRNDR_DEBUG_UTILS_INCLUDED
#define VKRNDR_DEBUG_UTILS_INCLUDED

#include <volk.h>

#include <array>
#include <span>
#include <string_view>

namespace vkrndr
{
    struct buffer_t;
    struct cubemap_t;
    struct device_t;
    struct image_t;
    struct pipeline_t;
    struct shader_module_t;
} // namespace vkrndr

namespace vkrndr
{
    struct [[nodiscard]] command_buffer_scope_t final
    {
    public:
        command_buffer_scope_t(VkCommandBuffer command_buffer,
            std::string_view label,
            std::span<float const, 3> const& color);

        command_buffer_scope_t(VkCommandBuffer command_buffer,
            std::string_view label,
            std::span<float const, 4> const& color =
                std::array{0.0f, 0.0f, 0.0f, 0.0f});

        command_buffer_scope_t(command_buffer_scope_t const&) = delete;

        command_buffer_scope_t(command_buffer_scope_t&&) noexcept = delete;

    public:
        ~command_buffer_scope_t();

    public:
        void close() noexcept;

    public:
        command_buffer_scope_t& operator=(
            command_buffer_scope_t const&) = delete;

        command_buffer_scope_t& operator=(
            command_buffer_scope_t&&) noexcept = delete;

    private:
        VkCommandBuffer command_buffer_;
    };

    void debug_label(VkCommandBuffer command_buffer,
        std::string_view label,
        std::span<float const, 3> const& color);

    void debug_label(VkCommandBuffer command_buffer,
        std::string_view label,
        std::span<float const, 4> const& color =
            std::array{0.0f, 0.0f, 0.0f, 0.0f});

    void object_name(device_t const& device,
        buffer_t const& buffer,
        std::string_view name);

    void object_name(device_t const& device,
        cubemap_t const& cubemap,
        std::string_view name);

    void object_name(device_t const& device,
        image_t const& image,
        std::string_view name);

    void object_name(device_t const& device,
        pipeline_t const& pipeline,
        std::string_view name);

    void object_name(device_t const& device,
        shader_module_t const& shader_module,
        std::string_view name);
} // namespace vkrndr

#endif
