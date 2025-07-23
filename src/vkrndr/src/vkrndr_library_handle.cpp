#include <vkrndr_library_handle.hpp>

#include <vkrndr_error_code.hpp>
#include <vkrndr_utility.hpp>

#include <volk.h>

#include <expected>

std::expected<vkrndr::library_handle_ptr_t, std::error_code>
vkrndr::initialize()
{
    if (VkResult const result{volkInitialize()}; !is_success_result(result))
    {
        return std::unexpected{vkrndr::make_error_code(result)};
    }

    return library_handle_ptr_t{new library_handle_t};
}

vkrndr::library_handle_t::~library_handle_t() { volkFinalize(); }
