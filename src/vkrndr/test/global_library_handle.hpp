#include <vkrndr_device.hpp>
#include <vkrndr_instance.hpp>
#include <vkrndr_library_handle.hpp>

namespace test
{
    // NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
    extern vkrndr::library_handle_ptr_t library;
    extern vkrndr::instance_ptr_t instance;
    extern vkrndr::device_ptr_t minimal_device;
    // NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)
} // namespace test
