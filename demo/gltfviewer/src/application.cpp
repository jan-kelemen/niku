#include <application.hpp>

#include <camera_controller.hpp>
#include <model_selector.hpp>
#include <pbr_renderer.hpp>

#include <cppext_numeric.hpp>
#include <cppext_overloaded.hpp>

#include <niku_application.hpp>
#include <niku_imgui_layer.hpp>
#include <niku_perspective_camera.hpp>
#include <niku_sdl_window.hpp> // IWYU pragma: keep

#include <vkgltf_loader.hpp>
#include <vkgltf_model.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_commands.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_render_settings.hpp>

#include <fmt/std.h> // IWYU pragma: keep

#include <imgui.h>

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_video.h>

#include <spdlog/spdlog.h>

#include <tl/expected.hpp>

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>
#include <memory>
#include <variant>
#include <utility>
#include <vector>

// IWYU pragma: no_include <fmt/base.h>

namespace
{
    [[nodiscard]] vkrndr::image_t create_color_image(
        vkrndr::backend_t const& backend)
    {
        return vkrndr::create_image_and_view(backend.device(),
            backend.extent(),
            1,
            VK_SAMPLE_COUNT_1_BIT,
            backend.image_format(),
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT);
    }
} // namespace

gltfviewer::application_t::application_t(bool const debug)
    : niku::application_t{niku::startup_params_t{
          .init_subsystems = {.video = true, .audio = false, .debug = debug},
          .title = "gltfviewer",
          .window_flags = static_cast<SDL_WindowFlags>(
              SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI),
          .centered = true,
          .width = 512,
          .height = 512}}
    , backend_{std::make_unique<vkrndr::backend_t>(*window(),
          vkrndr::render_settings_t{
              .preferred_present_mode = VK_PRESENT_MODE_FIFO_KHR},
          debug)}
    , imgui_{std::make_unique<niku::imgui_layer_t>(*window(),
          backend_->context(),
          backend_->device(),
          backend_->swap_chain())}
    , color_image_{create_color_image(*backend_)}
    , pbr_renderer_{std::make_unique<pbr_renderer_t>(backend_.get())}
    , camera_controller_{camera_, mouse_}
    , gltf_loader_{backend_.get()}
    
{
}

gltfviewer::application_t::~application_t() = default;

bool gltfviewer::application_t::handle_event(SDL_Event const& event)
{
    camera_controller_.handle_event(event);

    [[maybe_unused]] auto imgui_handled{imgui_->handle_event(event)};

    return true;
}

void gltfviewer::application_t::update(float delta_time)
{
    camera_controller_.update(delta_time);
}

void gltfviewer::application_t::begin_frame()
{
    imgui_->begin_frame();
    if (selector_.select_model())
    {
        std::filesystem::path const model_path{selector_.selected_model()};
        spdlog::info("Start loading: {}", model_path);

        auto model{gltf_loader_.load(model_path)};
        if (!model.has_value())
        {
            spdlog::error("Failed to load model: {}. Reason: {}",
                model_path,
                model.error().message());
            return;
        }

        spdlog::info("Loaded {} meshes, {} vertices, {} indices",
            model->meshes.size(),
            model->vertex_count,
            model->index_count);

        vkDeviceWaitIdle(backend_->device().logical);

        pbr_renderer_->load_model(std::move(model).value());

        spdlog::info("End loading: {}", model_path);
    }
}

bool gltfviewer::application_t::begin_draw()
{
    auto const rv{std::visit(
        cppext::overloaded{[](bool const acquired) { return acquired; },
            [this](VkExtent2D const extent)
            {
                on_resize(extent.width, extent.height);
                return false;
            }},
        backend_->begin_frame())};

    if (rv)
    {
        pbr_renderer_->update(camera_);
    }

    return rv;
}

void gltfviewer::application_t::draw()
{
    ImGui::ShowMetricsWindow();

    camera_controller_.draw_imgui();

    auto target_image{backend_->swapchain_image()};

    VkCommandBuffer command_buffer{backend_->request_command_buffer(false)};

    VkViewport const viewport{.x = 0.0f,
        .y = 0.0f,
        .width = cppext::as_fp(target_image.extent.width),
        .height = cppext::as_fp(target_image.extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f};
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D const scissor{{0, 0}, target_image.extent};
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    pbr_renderer_->draw(command_buffer, color_image_);

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

    VkOffset3D const size{
        .x = cppext::narrow<int32_t>(target_image.extent.width),
        .y = cppext::narrow<int32_t>(target_image.extent.height),
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

    imgui_->render(command_buffer, target_image);

    backend_->draw();
}

void gltfviewer::application_t::end_draw() { backend_->end_frame(); }

void gltfviewer::application_t::end_frame() { imgui_->end_frame(); }

void gltfviewer::application_t::on_shutdown()
{
    vkDeviceWaitIdle(backend_->device().logical);

    pbr_renderer_.reset();

    destroy(&backend_->device(), &color_image_);

    imgui_.reset();

    backend_.reset();
}

void gltfviewer::application_t::on_resize([[maybe_unused]] uint32_t width,
    [[maybe_unused]] uint32_t height)
{
    destroy(&backend_->device(), &color_image_);
    color_image_ = create_color_image(*backend_);

    pbr_renderer_->resize(width, height);
}
