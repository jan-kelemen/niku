#ifndef VKRNDR_DEBUG_UTILS_INCLUDED
#define VKRNDR_DEBUG_UTILS_INCLUDED

#include <volk.h>

#include <array>
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
} // namespace vkrndr

#endif