#include <application.hpp>

#include <text_editor.hpp>

#include <cppext_container.hpp>
#include <cppext_numeric.hpp>
#include <cppext_overloaded.hpp>

#include <ngnwsi_application.hpp>
#include <ngnwsi_imgui_layer.hpp>
#include <ngnwsi_mouse.hpp>
#include <ngnwsi_sdl_window.hpp>

#include <ngntxt_font_face.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_commands.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_library.hpp>
#include <vkrndr_render_pass.hpp>
#include <vkrndr_render_settings.hpp>
#include <vkrndr_synchronization.hpp>
#include <vkrndr_utility.hpp>

#include <imgui.h>

#include <volk.h>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_video.h>

#include <spdlog/spdlog.h>

#include <exception>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <variant>

// IWYU pragma: no_include  <filesystem>

reshed::application_t::application_t(bool const debug)
    : ngnwsi::application_t{ngnwsi::startup_params_t{
          .init_subsystems = {.video = true, .debug = debug},
          .title = "reshed",
          .window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY,
          .width = 512,
          .height = 512}}
    , freetype_context_{ngntxt::freetype_context_t::create()}
    , backend_{std::make_unique<vkrndr::backend_t>(vkrndr::initialize(),
          *window(),
          vkrndr::render_settings_t{
              .preferred_swapchain_format = VK_FORMAT_R8G8B8A8_UNORM,
              .swapchain_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                  VK_IMAGE_USAGE_STORAGE_BIT |
                  VK_IMAGE_USAGE_TRANSFER_DST_BIT,
              .preferred_present_mode = VK_PRESENT_MODE_FIFO_KHR,
          },
          debug)}
    , imgui_{std::make_unique<ngnwsi::imgui_layer_t>(*window(),
          backend_->instance(),
          backend_->device(),
          backend_->swap_chain())}
    , editor_{std::make_unique<text_editor_t>(*backend_)}
{
}

reshed::application_t::~application_t() = default;

bool reshed::application_t::handle_event(SDL_Event const& event)
{
    [[maybe_unused]] auto imgui_handled{imgui_->handle_event(event)};

    if (event.type == SDL_EVENT_KEY_DOWN &&
        event.key.scancode == SDL_SCANCODE_F4)
    {
        imgui_->set_enabled(!imgui_->enabled());
    }
    else if (event.type == SDL_EVENT_TEXT_INPUT ||
        event.type == SDL_EVENT_KEY_UP ||
        event.type == SDL_EVENT_KEY_DOWN)
    {
        editor_->handle_event(event);
    }

    return true;
}

void reshed::application_t::update([[maybe_unused]] float const delta_time)
{
    ImGui::ShowMetricsWindow();

    editor_->update();
}

bool reshed::application_t::begin_frame()
{
    auto const on_swapchain_acquire = [this](bool const acquired)
    { return acquired; };

    auto const on_swapchain_resized = [this](VkExtent2D const extent)
    {
        editor_->resize(extent.width, extent.height);
        return false;
    };

    auto const rv{std::visit(
        cppext::overloaded{on_swapchain_acquire, on_swapchain_resized},
        backend_->begin_frame())};

    imgui_->begin_frame();

    return rv;
}

void reshed::application_t::draw()
{
    auto target_image{backend_->swapchain_image()};

    VkCommandBuffer command_buffer{backend_->request_command_buffer()};

    vkrndr::wait_for_color_attachment_write(target_image.image, command_buffer);

    VkViewport const viewport{.x = 0.0f,
        .y = cppext::as_fp(target_image.extent.height),
        .width = cppext::as_fp(target_image.extent.width),
        .height = -cppext::as_fp(target_image.extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f};
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D const scissor{{0, 0}, vkrndr::to_2d_extent(target_image.extent)};
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    vkrndr::render_pass_t color_render_pass;
    color_render_pass.with_color_attachment(VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        target_image.view,
        VkClearValue{.color = {{0.0f, 0.0f, 0.0f, 1.0f}}});

    {
        [[maybe_unused]] auto guard{color_render_pass.begin(command_buffer,
            {{0, 0}, vkrndr::to_2d_extent(target_image.extent)})};
        editor_->draw(command_buffer);
    }

    {
        auto const barrier{vkrndr::with_access(
            vkrndr::on_stage(vkrndr::image_barrier(target_image),
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT),
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT)};

        vkrndr::wait_for(command_buffer, {}, {}, cppext::as_span(barrier));
    }

    imgui_->render(command_buffer, target_image);

    vkrndr::transition_to_present_layout(target_image.image, command_buffer);

    backend_->draw();
}

void reshed::application_t::end_frame()
{
    imgui_->end_frame();

    backend_->end_frame();
}

void reshed::application_t::on_startup()
{
    mouse_.set_window_handle(window()->native_handle());

    if (std::expected<ngntxt::font_face_ptr_t, std::error_code> font_face{
            load_font_face(freetype_context_,
                "SpaceMono-Regular.ttf",
                {0, 16})})
    {
        spdlog::info("Font loaded");
        editor_->change_font(std::move(font_face).value());
    }
    else
    {
        std::terminate();
    }

    text_input_guard_ =
        std::make_unique<ngnwsi::sdl_text_input_guard_t>(*window());
}

void reshed::application_t::on_shutdown()
{
    text_input_guard_.reset();

    vkDeviceWaitIdle(backend_->device().logical);

    editor_.reset();

    imgui_.reset();

    backend_.reset();
}
