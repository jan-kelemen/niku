#ifndef VKRNDR_WINDOW_INCLUDED
#define VKRNDR_WINDOW_INCLUDED

#include <volk.h>

#include <vector>

namespace vkrndr
{
    // NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
    class [[nodiscard]] window_t
    {
    public: // Destruction
        virtual ~window_t() = default;

    public: // Interface
        [[nodiscard]] virtual VkSurfaceKHR create_surface(
            VkInstance instance) = 0;

        virtual void destroy_surface(VkInstance instance) = 0;

        [[nodiscard]] virtual VkSurfaceKHR surface() const = 0;

        [[nodiscard]] virtual VkExtent2D swap_extent(
            VkSurfaceCapabilitiesKHR const& capabilities) const = 0;
    };
} // namespace vkrndr

#endif // !VKRNDR_VULKAN_WINDOW_INCLUDED
