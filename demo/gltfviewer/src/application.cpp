#include <application.hpp>

#include <cppext_numeric.hpp>

#include <niku_application.hpp>
#include <niku_mouse.hpp>

#include <vkrndr_render_pass.hpp>
#include <vkrndr_scene.hpp>
#include <vkrndr_vulkan_commands.hpp>
#include <vkrndr_vulkan_renderer.hpp>

#include <imgui.h>

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_scancode.h>
#include <SDL2/SDL_video.h>

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <memory>

namespace gltfviewer
{
    class [[nodiscard]] scene final : public vkrndr::scene
    {
    public:
        scene(vkrndr::vulkan_device* const device,
            vkrndr::vulkan_renderer* const renderer,
            VkExtent2D const extent)
            : device_{device}
            , renderer_{renderer}
        {
            create_color_image(extent);
        }

        scene(scene const&) = delete;

        scene(scene&&) noexcept = delete;

    public:
        ~scene() override { destroy(device_, &color_image_); }

    public: // vkrndr::scene overrides
        void resize(VkExtent2D const extent) override
        {
            destroy(device_, &color_image_);
            create_color_image(extent);
        }

        void draw(vkrndr::vulkan_image const& target_image,
            VkCommandBuffer command_buffer,
            VkExtent2D extent) override
        {
            VkViewport const viewport{.x = 0.0f,
                .y = 0.0f,
                .width = cppext::as_fp(extent.width),
                .height = cppext::as_fp(extent.height),
                .minDepth = 0.0f,
                .maxDepth = 1.0f};
            vkCmdSetViewport(command_buffer, 0, 1, &viewport);

            VkRect2D const scissor{{0, 0}, extent};
            vkCmdSetScissor(command_buffer, 0, 1, &scissor);

            vkrndr::transition_image(color_image_.image,
                command_buffer,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_PIPELINE_STAGE_2_BLIT_BIT,
                VK_ACCESS_2_TRANSFER_READ_BIT,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                1);

            {
                vkrndr::render_pass color_render_pass;
                color_render_pass.with_color_attachment(
                    VK_ATTACHMENT_LOAD_OP_CLEAR,
                    VK_ATTACHMENT_STORE_OP_STORE,
                    color_image_.view,
                    VkClearValue{.color = {1.0f, 0.5f, 0.5f}});

                auto guard{
                    color_render_pass.begin(command_buffer, {{0, 0}, extent})};
            }

            vkrndr::transition_image(color_image_.image,
                command_buffer,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_PIPELINE_STAGE_2_BLIT_BIT,
                VK_ACCESS_2_TRANSFER_READ_BIT,
                1);

            vkrndr::transition_image(target_image.image,
                command_buffer,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_2_NONE,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                VK_ACCESS_2_TRANSFER_WRITE_BIT,
                1);

            VkOffset3D size{.x = cppext::narrow<int32_t>(extent.width),
                .y = cppext::narrow<int32_t>(extent.height),
                .z = 1};
            VkImageBlit region{};
            region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.srcSubresource.layerCount = 1;
            region.srcOffsets[1] = size;
            region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.dstSubresource.layerCount = 1;
            region.dstOffsets[1] = size;

            vkCmdBlitImage(command_buffer,
                color_image_.image,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                target_image.image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &region,
                VK_FILTER_NEAREST);

            vkrndr::transition_image(target_image.image,
                command_buffer,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_2_BLIT_BIT,
                VK_ACCESS_2_TRANSFER_WRITE_BIT,
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                VK_ACCESS_2_NONE,
                1);
        }

        void draw_imgui() override { ImGui::ShowMetricsWindow(); }

    public:
        scene& operator=(scene const&) = delete;

        scene& operator=(scene&&) = delete;

    private:
        void create_color_image(VkExtent2D const extent)
        {
            color_image_ = vkrndr::create_image_and_view(*device_,
                extent,
                1,
                VK_SAMPLE_COUNT_1_BIT,
                renderer_->image_format(),
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT);
        }

    private:
        vkrndr::vulkan_device* device_;
        vkrndr::vulkan_renderer* renderer_;

        vkrndr::vulkan_image color_image_;
    };
} // namespace gltfviewer

gltfviewer::application::application(bool const debug)
    : niku::application{niku::startup_params{
          .init_subsystems = {.video = true, .audio = false, .debug = debug},
          .title = "gltfviewer",
          .window_flags = static_cast<SDL_WindowFlags>(
              SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI),
          .centered = true,
          .width = 512,
          .height = 512,
          .render = {.preferred_present_mode = VK_PRESENT_MODE_FIFO_KHR}}}
    , scene_{std::make_unique<scene>(this->vulkan_device(),
          this->vulkan_renderer(),
          this->vulkan_renderer()->extent())}
{
    this->vulkan_renderer()->imgui_layer(true);
}

gltfviewer::application::~application() = default;

bool gltfviewer::application::handle_event(
    [[maybe_unused]] SDL_Event const& event)
{
    return false;
}

void gltfviewer::application::update([[maybe_unused]] float delta_time) { }

vkrndr::scene* gltfviewer::application::render_scene() { return scene_.get(); }
