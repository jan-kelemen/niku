#ifndef VKRNDR_ERROR_CODE_INCLUDED
#define VKRNDR_ERROR_CODE_INCLUDED

#include <volk.h>

#include <system_error>
#include <utility>

namespace vkrndr
{
    [[nodiscard]] std::error_code make_error_code(VkResult result);

    [[nodiscard]] constexpr bool is_success_result(VkResult result) noexcept
    {
        return std::to_underlying(result) >= 0;
    }
} // namespace vkrndr

template<>
struct std::is_error_code_enum<VkResult> : std::true_type
{
};

#endif
