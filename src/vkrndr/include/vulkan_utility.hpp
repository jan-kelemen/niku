#ifndef VKRNDR_VULKAN_UTILITY_INCLUDED
#define VKRNDR_VULKAN_UTILITY_INCLUDED

#include <cppext_numeric.hpp>

#include <vulkan/vulkan_core.h>

#include <algorithm> // IWYU pragma: keep
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <source_location>
#include <span>
#include <vector>

namespace vkrndr
{
    VkResult check_result(VkResult result,
        std::source_location source_location = std::source_location::current());

    template<typename T>
    [[nodiscard]] constexpr uint32_t count_cast(T const count)
    {
        return cppext::narrow<uint32_t>(count);
    }

    template<typename T>
    [[nodiscard]] std::span<std::byte const> as_bytes(T const& value,
        size_t const size = sizeof(T))
    {
        // NOLINTNEXTLINE
        return {reinterpret_cast<std::byte const*>(&value), size};
    }

    template<typename T>
    [[nodiscard]] std::span<std::byte const> as_bytes(
        std::vector<T> const& value,
        std::optional<size_t> const elements = std::nullopt)
    {
        // NOLINTNEXTLINE
        return {reinterpret_cast<std::byte const*>(value.data()),
            elements.value_or(value.size()) * sizeof(T)};
    }

    template<typename T>
    [[nodiscard]] uint32_t max_mip_levels(T const width, T const height)
    {
        return static_cast<uint32_t>(
                   std::floor(std::log2(std::max(width, height)))) +
            1;
    }
} // namespace vkrndr

#endif // !VKRNDR_VULKAN_UTILITY_INCLUDED
