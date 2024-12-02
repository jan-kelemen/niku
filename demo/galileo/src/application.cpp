#include <application.hpp>

#include <camera_controller.hpp>

#include <cppext_numeric.hpp>
#include <cppext_overloaded.hpp>
#include <cppext_pragma_warning.hpp>

#include <niku_application.hpp>
#include <niku_imgui_layer.hpp>
#include <niku_perspective_camera.hpp>
#include <niku_sdl_window.hpp> // IWYU pragma: keep

#include <vkrndr_backend.hpp>
#include <vkrndr_commands.hpp>
#include <vkrndr_debug_utils.hpp>
#include <vkrndr_depth_buffer.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_render_settings.hpp>

DISABLE_WARNING_PUSH
DISABLE_WARNING_STRINGOP_OVERFLOW
#include <fmt/std.h> // IWYU pragma: keep
DISABLE_WARNING_POP

#include <imgui.h>

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_scancode.h>
#include <SDL2/SDL_video.h>

#include <spdlog/spdlog.h>

#include <volk.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

galileo::application_t::application_t(bool const debug)
    : niku::application_t{niku::startup_params_t{
          .init_subsystems = {.video = true, .debug = debug},
          .title = "galileo",
          .window_flags = static_cast<SDL_WindowFlags>(
              SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI),
          .centered = true,
          .width = 512,
          .height = 512}}
    , backend_{std::make_unique<vkrndr::backend_t>(*window(),
          vkrndr::render_settings_t{
              .preferred_swapchain_format = VK_FORMAT_R8G8B8A8_SRGB,
              .preferred_present_mode = VK_PRESENT_MODE_FIFO_KHR,
          },
          debug)}
    , imgui_{std::make_unique<niku::imgui_layer_t>(*window(),
          backend_->context(),
          backend_->device(),
          backend_->swap_chain())}
    , camera_controller_{camera_, mouse_}
{
}

galileo::application_t::~application_t() = default;

bool galileo::application_t::handle_event(SDL_Event const& event)
{
    camera_controller_.handle_event(event);

    [[maybe_unused]] auto imgui_handled{imgui_->handle_event(event)};

    if (event.type == SDL_KEYDOWN)
    {
        auto const& keyboard{event.key};
        if (keyboard.keysym.scancode == SDL_SCANCODE_F4)
        {
            imgui_->set_enabled(!imgui_->enabled());
        }
    }

    return true;
}

void galileo::application_t::update(float delta_time)
{
    camera_controller_.update(delta_time);
}

bool galileo::application_t::begin_frame()
{
    auto const on_swapchain_acquire = [this](bool const acquired)
    {
        if (acquired)
        {
            imgui_->begin_frame();
        }
        return acquired;
    };

    auto const on_swapchain_resized = [this](VkExtent2D const extent)
    {
        on_resize(extent.width, extent.height);
        return false;
    };

    return std::visit(
        cppext::overloaded{on_swapchain_acquire, on_swapchain_resized},
        backend_->begin_frame());
}

void galileo::application_t::draw()
{
    ImGui::ShowMetricsWindow();

    camera_controller_.draw_imgui();

    auto target_image{backend_->swapchain_image()};

    VkCommandBuffer command_buffer{backend_->request_command_buffer()};

    vkrndr::wait_for_color_attachment_read(target_image.image, command_buffer);

    VkViewport const viewport{.x = 0.0f,
        .y = 0.0f,
        .width = cppext::as_fp(target_image.extent.width),
        .height = cppext::as_fp(target_image.extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f};
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D const scissor{{0, 0}, target_image.extent};
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    imgui_->render(command_buffer, target_image);

    vkrndr::transition_to_present_layout(target_image.image, command_buffer);

    backend_->draw();
}

void galileo::application_t::end_frame()
{
    imgui_->end_frame();

    backend_->end_frame();
}

void galileo::application_t::on_startup()
{
    auto const extent{backend_->extent()};
    on_resize(extent.width, extent.height);
}

void galileo::application_t::on_shutdown()
{
    vkDeviceWaitIdle(backend_->device().logical);

    imgui_.reset();

    backend_.reset();
}

void galileo::application_t::on_resize(uint32_t const width,
    uint32_t const height)
{
    camera_.set_aspect_ratio(cppext::as_fp(width) / cppext::as_fp(height));
}
