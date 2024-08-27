#ifndef VKRNDR_SCENE_INCLUDED
#define VKRNDR_SCENE_INCLUDED

#include <vulkan/vulkan_core.h>

namespace vkrndr
{
    struct image_t;
} // namespace vkrndr

namespace vkrndr
{
    // NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
    class [[nodiscard]] scene_t
    {
    public: // Destruction
        virtual ~scene_t() = default;

    public: // Interface
        virtual void draw(image_t const& target_image,
            VkCommandBuffer command_buffer,
            VkExtent2D extent) = 0;

        virtual void draw_imgui() = 0;
    };
} // namespace vkrndr

#endif
