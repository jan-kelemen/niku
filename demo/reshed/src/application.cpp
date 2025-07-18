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
          .init_subsystems = {.video = true, .debug = debug}}}
    , freetype_context_{ngntxt::freetype_context_t::create()}
    , render_window_{std::make_unique<ngnwsi::render_window_t>("reshed",
          SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY,
          512,
          512)}
{
    backend_ = std::make_unique<vkrndr::backend_t>(
        vkrndr::initialize(),
        ngnwsi::sdl_window_t::required_extensions(),
        [this](VkInstance instance)
        { return render_window_->create_surface(instance); },
        2,
        debug);

    vkrndr::swapchain_t* const swapchain{
        render_window_->create_swapchain(backend_->device(),
            {
                .preffered_format = VK_FORMAT_R8G8B8A8_UNORM,
                .image_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                    VK_IMAGE_USAGE_STORAGE_BIT |
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                .preferred_present_mode = VK_PRESENT_MODE_FIFO_KHR,
            })};

    imgui_ = std::make_unique<ngnwsi::imgui_layer_t>(
        render_window_->platform_window(),
        backend_->instance(),
        backend_->device(),
        *swapchain);

    editor_ =
        std::make_unique<text_editor_t>(*backend_, swapchain->image_format());
}

reshed::application_t::~application_t() = default;

bool reshed::application_t::should_run()
{
    return static_cast<bool>(render_window_);
}

bool reshed::application_t::handle_event(SDL_Event const& event)
{
    [[maybe_unused]] auto imgui_handled{imgui_->handle_event(event)};

    if (event.type == SDL_EVENT_QUIT ||
        event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
    {
        if (render_window_)
        {
            text_input_guard_.reset();

            vkDeviceWaitIdle(backend_->device().logical);

            render_window_->destroy_swapchain();
            render_window_->destroy_surface(backend_->instance().handle);
            render_window_.reset();
        }

        return true;
    }

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

void reshed::application_t::draw()
{
    std::optional<vkrndr::image_t> const target_image{
        render_window_->acquire_next_image()};
    if (!target_image)
    {
        return;
    }

    backend_->begin_frame();

    imgui_->begin_frame();

    ImGui::ShowMetricsWindow();

    editor_->debug_draw();

    VkCommandBuffer command_buffer{backend_->request_command_buffer()};

    vkrndr::wait_for_color_attachment_write(target_image->image,
        command_buffer);

    VkViewport const viewport{.x = 0.0f,
        .y = cppext::as_fp(target_image->extent.height),
        .width = cppext::as_fp(target_image->extent.width),
        .height = -cppext::as_fp(target_image->extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f};
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D const scissor{{0, 0}, vkrndr::to_2d_extent(target_image->extent)};
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    vkrndr::render_pass_t color_render_pass;
    color_render_pass.with_color_attachment(VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        target_image->view,
        VkClearValue{.color = {{0.0f, 0.0f, 0.0f, 1.0f}}});

    {
        [[maybe_unused]] auto guard{color_render_pass.begin(command_buffer,
            {{0, 0}, vkrndr::to_2d_extent(target_image->extent)})};
        editor_->draw(command_buffer);
    }

    {
        auto const barrier{vkrndr::with_access(
            vkrndr::on_stage(vkrndr::image_barrier(*target_image),
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT),
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT)};

        vkrndr::wait_for(command_buffer, {}, {}, cppext::as_span(barrier));
    }

    imgui_->render(command_buffer, *target_image);

    vkrndr::transition_to_present_layout(target_image->image, command_buffer);

    render_window_->present(backend_->present_buffers());

    backend_->end_frame();
}

void reshed::application_t::end_frame() { imgui_->end_frame(); }

void reshed::application_t::on_startup()
{
    mouse_.set_window_handle(render_window_->platform_window().native_handle());

    auto const extent{render_window_->swapchain().extent()};
    on_resize(extent.width, extent.height);

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

    text_input_guard_ = std::make_unique<ngnwsi::sdl_text_input_guard_t>(
        render_window_->platform_window());
}

void reshed::application_t::on_shutdown()
{
    text_input_guard_.reset();

    vkDeviceWaitIdle(backend_->device().logical);

    editor_.reset();

    imgui_.reset();

    if (render_window_)
    {
        render_window_->destroy_swapchain();
        render_window_->destroy_surface(backend_->instance().handle);
        render_window_.reset();
    }

    backend_.reset();
}

void reshed::application_t::on_resize(uint32_t width, uint32_t height)
{
    vkDeviceWaitIdle(backend_->device().logical);

    render_window_->swapchain().recreate();

    editor_->resize(width, height);
}
