#ifndef VKRNDR_DEBUG_UTILS_INCLUDED
#define VKRNDR_DEBUG_UTILS_INCLUDED

#include <volk.h>

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

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

    void object_name(VkDevice device,
        VkObjectType type,
        uint64_t handle,
        std::string_view name);
} // namespace vkrndr

#if VKRNDR_ENABLE_DEBUG_UTILS
#define VKRNDR_IF_DEBUG_UTILS(...) __VA_ARGS__
#else
#define VKRNDR_IF_DEBUG_UTILS
#endif

#endif
