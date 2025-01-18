#ifndef VKRNDR_CONTEXT_INCLUDED
#define VKRNDR_CONTEXT_INCLUDED

#include <volk.h>

#include <span>

namespace vkrndr
{
    struct [[nodiscard]] context_t final
    {
        VkInstance instance{VK_NULL_HANDLE};
        VkDebugUtilsMessengerEXT debug_messenger{VK_NULL_HANDLE};
    };

    context_t create_context(bool setup_validation_layers,
        std::span<char const* const> const& required_extensions);

    void destroy(context_t* context);
} // namespace vkrndr

#endif
