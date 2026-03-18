#ifndef VKRNDR_TEST_HELPERS_INCLUDED
#define VKRNDR_TEST_HELPERS_INCLUDED

#include <vkrndr_features.hpp>
#include <vkrndr_utility.hpp>

#include <algorithm>
#include <ranges>

[[nodiscard]] vkrndr::queue_family_t const* find_general_queue(
    std::ranges::forward_range auto&& families)
{
    auto const is_general_queue = [](vkrndr::queue_family_t const& q)
    {
        return vkrndr::supports_flags(q.properties.queueFlags,
            VK_QUEUE_GRAPHICS_BIT |
                VK_QUEUE_TRANSFER_BIT |
                VK_QUEUE_COMPUTE_BIT);
    };

    if (auto const queue_it{std::ranges::find_if(families, is_general_queue)};
        queue_it != cend(families))
    {
        return &(*queue_it);
    }

    return nullptr;
}
#endif
