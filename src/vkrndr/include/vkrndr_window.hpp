#ifndef VKRNDR_WINDOW_INCLUDED
#define VKRNDR_WINDOW_INCLUDED

#include <vulkan/vulkan_core.h>

#include <vector>

namespace vkrndr
{
    // NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
    class [[nodiscard]] window_t
    {
    public: // Destruction
        virtual ~window_t() = default;

    public: // Interface
        [[nodiscard]] virtual std::vector<char const*>
        required_extensions() const = 0;

        [[nodiscard]] virtual VkResult create_surface(VkInstance instance,
            VkSurfaceKHR& surface) const = 0;

        [[nodiscard]] virtual VkExtent2D swap_extent(
            VkSurfaceCapabilitiesKHR const& capabilities) const = 0;

        [[nodiscard]] virtual bool is_minimized() const = 0;

        virtual void init_imgui() = 0;

        virtual void new_imgui_frame() = 0;

        virtual void shutdown_imgui() = 0;
    };

} // namespace vkrndr

#endif // !VKRNDR_VULKAN_WINDOW_INCLUDED
