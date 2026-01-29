#include <vkrndr_rendering_context.hpp>

void vkrndr::destroy(rendering_context_t& context)
{
    context.device.reset();
    context.instance.reset();
    context.library_handle.reset();
}
