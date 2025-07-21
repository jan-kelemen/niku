#ifndef VKRNDR_LIBRARY_INCLUDED
#define VKRNDR_LIBRARY_INCLUDED

#include <memory>

namespace vkrndr
{
    struct [[nodiscard]] library_handle_t;

    std::shared_ptr<library_handle_t> initialize();

    [[nodiscard]] bool is_instance_extension_available(
        library_handle_t const& handle,
        char const* extension_name,
        char const* layer_name = nullptr);
} // namespace vkrndr

#endif
