#ifndef VKRNDR_CONTEXT_INCLUDED
#define VKRNDR_CONTEXT_INCLUDED

#include <volk.h>

#include <cstdint>
#include <span>

namespace vkrndr
{
    struct [[nodiscard]] context_t final
    {
        VkInstance instance{VK_NULL_HANDLE};
    };

    void destroy(context_t* context);

    struct [[nodiscard]] context_create_info_t final
    {
        void const* chain{};

        uint32_t minimal_vulkan_version{};
        std::span<char const* const> extensions;
        std::span<char const* const> layers;

        char const* application_name{};
        uint32_t application_version{};
    };

    context_t create_context(context_create_info_t const& create_info);
} // namespace vkrndr

#endif
