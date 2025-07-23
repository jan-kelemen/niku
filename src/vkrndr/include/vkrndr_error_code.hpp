#ifndef VKRNDR_ERROR_CODE_INCLUDED
#define VKRNDR_ERROR_CODE_INCLUDED

#include <volk.h>

#include <system_error>
#include <type_traits>

namespace vkrndr
{
    [[nodiscard]] std::error_code make_error_code(VkResult result);
} // namespace vkrndr

template<>
struct std::is_error_code_enum<VkResult> : std::true_type
{
};

#endif
