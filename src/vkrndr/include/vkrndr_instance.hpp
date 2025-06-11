#ifndef VKRNDR_INSTANCE_INCLUDED
#define VKRNDR_INSTANCE_INCLUDED

#include <volk.h>

#include <cstdint>
#include <functional>
#include <set>
#include <span>
#include <string>

namespace vkrndr
{
    struct [[nodiscard]] instance_t final
    {
        VkInstance handle{VK_NULL_HANDLE};

        std::set<std::string, std::less<>> layers;
        std::set<std::string, std::less<>> extensions;
    };

    void destroy(instance_t* instance);

    struct [[nodiscard]] instance_create_info_t final
    {
        void const* chain{};

        uint32_t maximum_vulkan_version{};
        std::span<char const* const> extensions;
        std::span<char const* const> layers;

        char const* application_name{};
        uint32_t application_version{};
    };

    instance_t create_instance(instance_create_info_t const& create_info);
} // namespace vkrndr

#endif
