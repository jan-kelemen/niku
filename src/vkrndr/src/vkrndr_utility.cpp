#include <vkrndr_utility.hpp>

#include <spdlog/spdlog.h>

#include <exception>
#include <source_location>
#include <utility>

// IWYU pragma: no_include <fmt/base.h>
// IWYU pragma: no_include <fmt/format.h>
// IWYU pragma: no_include <spdlog/common.h>

VkResult vkrndr::check_result(VkResult const result,
    std::source_location const source_location)
{
    if (result == VK_SUCCESS)
    {
        return result;
    }

    if (std::to_underlying(result) < 0)
    {
        spdlog::error("VkResult = {}. {}:{}",
            std::to_underlying(result),
            source_location.file_name(),
            source_location.line());
        std::terminate();
    }
    else
    {
        spdlog::warn("VkResult = {}. {}:{}",
            std::to_underlying(result),
            source_location.file_name(),
            source_location.line());
    }

    return result;
}
