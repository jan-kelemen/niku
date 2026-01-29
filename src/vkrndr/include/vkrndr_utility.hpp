#ifndef VKRNDR_UTILITY_INCLUDED
#define VKRNDR_UTILITY_INCLUDED

#include <cppext_numeric.hpp>

#include <volk.h>

#include <algorithm> // IWYU pragma: keep
#include <cmath>
#include <concepts>
#include <cstdint>
#include <ranges>
#include <source_location>
#include <utility>

#if VK_USE_64_BIT_PTR_DEFINES
#include <bit>
#endif

namespace vkrndr
{
    VkResult check_result(VkResult result,
        std::source_location source_location = std::source_location::current());

    template<typename T>
    [[nodiscard]] constexpr uint64_t handle_cast(T const handle) noexcept
    {
#if VK_USE_64_BIT_PTR_DEFINES == 1
        return std::bit_cast<uint64_t>(handle);
#else
        return handle;
#endif
    }

    [[nodiscard]] constexpr bool is_success_result(VkResult result) noexcept
    {
        return std::to_underlying(result) >= 0;
    }

    template<std::integral T>
    [[nodiscard]] constexpr uint32_t count_cast(T const count)
    {
        return cppext::narrow<uint32_t>(count);
    }

    template<typename Container>
    [[nodiscard]] constexpr uint32_t count_cast(Container const& container)
    requires(std::ranges::contiguous_range<Container> &&
        std::ranges::sized_range<Container>)
    {
        return cppext::narrow<uint32_t>(std::ranges::size(container));
    }

    template<std::integral T>
    [[nodiscard]] constexpr VkExtent2D to_2d_extent(T const width,
        T const height = 1)
    {
        return {cppext::narrow<uint32_t>(width),
            cppext::narrow<uint32_t>(height)};
    }

    [[nodiscard]] constexpr VkExtent2D to_2d_extent(VkExtent3D const extent)
    {
        return {extent.width, extent.height};
    }

    template<std::integral T>
    [[nodiscard]] constexpr VkExtent3D to_3d_extent(T const extent)
    {
        return {cppext::narrow<uint32_t>(extent), 1, 1};
    }

    template<std::integral T>
    [[nodiscard]] constexpr VkExtent3D
    to_3d_extent(T const width, T const height = 1, T const depth = 1)
    {
        return {cppext::narrow<uint32_t>(width),
            cppext::narrow<uint32_t>(height),
            cppext::narrow<uint32_t>(depth)};
    }

    template<std::integral T = uint32_t>
    [[nodiscard]] constexpr VkExtent3D to_3d_extent(VkExtent2D const extent,
        T const depth = 1)
    {
        return {extent.width, extent.height, cppext::narrow<uint32_t>(depth)};
    }

    template<std::integral T>
    [[nodiscard]] uint32_t max_mip_levels(T const width, T const height)
    {
        return static_cast<uint32_t>(
                   std::floor(std::log2(std::max(width, height)))) +
            1;
    }

    template<typename... Args>
    [[nodiscard]] constexpr bool supports_flags(VkFlags const flags,
        Args... bits)
    {
        auto const all_bits{static_cast<VkFlags>((bits | ...))};
        return (flags & all_bits) == all_bits;
    }
} // namespace vkrndr

#endif
