#include <vkrndr_library_handle.hpp>

#include <vkrndr_error_code.hpp>
#include <vkrndr_features.hpp>
#include <vkrndr_utility.hpp>

#include <volk.h>

#include <algorithm>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

std::expected<vkrndr::library_handle_ptr_t, std::error_code>
vkrndr::initialize()
{
    auto temp{std::make_unique<library_handle_t>()};

    if (VkResult const result{volkInitialize()}; !is_success_result(result))
    {
        return std::unexpected{vkrndr::make_error_code(result)};
    }

    return library_handle_ptr_t{temp.release()};
}

vkrndr::library_handle_t::~library_handle_t() { volkFinalize(); }
