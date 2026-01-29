#ifndef VKRNDR_RENDERING_CONTEXT_INCLUDED
#define VKRNDR_RENDERING_CONTEXT_INCLUDED

#include <vkrndr_device.hpp>
#include <vkrndr_instance.hpp>
#include <vkrndr_library_handle.hpp>

namespace vkrndr
{
    struct rendering_context_t final
    {
        library_handle_ptr_t library_handle;
        instance_ptr_t instance;
        device_ptr_t device;
    };

    void destroy(rendering_context_t& context);
} // namespace vkrndr
#endif
