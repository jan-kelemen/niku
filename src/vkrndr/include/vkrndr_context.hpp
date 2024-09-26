#ifndef VKRNDR_CONTEXT_INCLUDED
#define VKRNDR_CONTEXT_INCLUDED

#include <volk.h>

namespace vkrndr
{
    class window_t;
} // namespace vkrndr

namespace vkrndr
{
    struct [[nodiscard]] context_t final
    {
        VkInstance instance{VK_NULL_HANDLE};
        VkSurfaceKHR surface{VK_NULL_HANDLE};
        VkDebugUtilsMessengerEXT debug_messenger{VK_NULL_HANDLE};
    };

    context_t create_context(window_t const& window,
        bool setup_validation_layers);

    void destroy(context_t* context);
} // namespace vkrndr

#endif
