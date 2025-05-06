#include <application.hpp>

#include <cppext_container.hpp>
#include <cppext_numeric.hpp>
#include <cppext_overloaded.hpp>

#include <ngnwsi_application.hpp>
#include <ngnwsi_imgui_layer.hpp>
#include <ngnwsi_mouse.hpp>
#include <ngnwsi_sdl_window.hpp>

#include <ngntxt_font_bitmap.hpp>
#include <ngntxt_font_face.hpp>
#include <ngntxt_shaping.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_commands.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_image.hpp>
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
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <variant>

reshed::application_t::application_t(bool const debug)
    : ngnwsi::application_t{ngnwsi::startup_params_t{
          .init_subsystems = {.video = true, .debug = debug},
          .title = "reshed",
          .window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY,
          .width = 512,
          .height = 512}}
    , backend_{std::make_unique<vkrndr::backend_t>(*window(),
          vkrndr::render_settings_t{
              .preferred_swapchain_format = VK_FORMAT_R8G8B8A8_UNORM,
              .swapchain_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                  VK_IMAGE_USAGE_STORAGE_BIT |
                  VK_IMAGE_USAGE_TRANSFER_DST_BIT,
              .preferred_present_mode = VK_PRESENT_MODE_FIFO_KHR,
          },
          debug)}
    , imgui_{std::make_unique<ngnwsi::imgui_layer_t>(*window(),
          backend_->context(),
          backend_->device(),
          backend_->swap_chain())}
{
}

reshed::application_t::~application_t() = default;

bool reshed::application_t::handle_event(SDL_Event const& event,
    [[maybe_unused]] float const delta_time)
{
    [[maybe_unused]] auto imgui_handled{imgui_->handle_event(event)};

    if (event.type == SDL_EVENT_KEY_DOWN)
    {
        auto const& keyboard{event.key};
        if (keyboard.scancode == SDL_SCANCODE_F4)
        {
            imgui_->set_enabled(!imgui_->enabled());
        }
    }

    return true;
}

bool reshed::application_t::begin_frame()
{
    auto const on_swapchain_acquire = [this](bool const acquired)
    {
        if (acquired)
        {
            imgui_->begin_frame();
        }
        return acquired;
    };

    auto const on_swapchain_resized =
        []([[maybe_unused]] VkExtent2D const extent) { return false; };

    return std::visit(
        cppext::overloaded{on_swapchain_acquire, on_swapchain_resized},
        backend_->begin_frame());
}

void reshed::application_t::draw()
{
    ImGui::ShowMetricsWindow();

    auto target_image{backend_->swapchain_image()};

    VkCommandBuffer command_buffer{backend_->request_command_buffer()};

    vkrndr::wait_for_color_attachment_write(target_image.image, command_buffer);

    VkViewport const viewport{.x = 0.0f,
        .y = 0.0f,
        .width = cppext::as_fp(target_image.extent.width),
        .height = cppext::as_fp(target_image.extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f};
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D const scissor{{0, 0}, vkrndr::to_2d_extent(target_image.extent)};
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    vkrndr::render_pass_t color_render_pass;
    color_render_pass.with_color_attachment(VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        target_image.view,
        VkClearValue{.color = {{1.0f, 0.0f, 0.0f, 1.0f}}});

    {
        [[maybe_unused]] auto guard{color_render_pass.begin(command_buffer,
            {{0, 0}, vkrndr::to_2d_extent(target_image.extent)})};
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

    auto scale{SDL_GetWindowPixelDensity(window()->native_handle())};
    if (scale == 0.0f)
    {
        scale = 1.0f;
    }

    glm::uvec2 actual_extent{};

    {
        int width{};
        int height{};
        if (SDL_GetWindowSize(window()->native_handle(), &width, &height))
        {
            actual_extent.x = cppext::narrow<unsigned>(width);
            actual_extent.y = cppext::narrow<unsigned>(height);
        }
        else
        {
            auto const extent{backend_->extent()};
            actual_extent.x = extent.width;
            actual_extent.y = extent.height;
        }

        actual_extent.x =
            static_cast<unsigned>(roundf(cppext::as_fp(width) * scale));
        actual_extent.y =
            static_cast<unsigned>(roundf(cppext::as_fp(height) * scale));
    }

    auto context{ngntxt::freetype_context_t::create()};
    if (std::expected<ngntxt::font_face_ptr_t, std::error_code> font_face{
            load_font_face(context,
                "SpaceMono-Regular.ttf",
                {0, 16},
                actual_extent)})
    {
        spdlog::info("Font loaded");
        font_bitmap_ = ngntxt::create_bitmap(*backend_, **font_face, 0, 256);

        ngntxt::shaping_font_face_ptr_t shaping_face{
            ngntxt::create_shaping_font_face(*font_face)};

        ngntxt::shaping_buffer_ptr_t shaping_buffer{
            ngntxt::create_shaping_buffer()};

        hb_buffer_add_utf8(shaping_buffer.get(), "Font loaded", -1, 0, -1);
        hb_buffer_guess_segment_properties(shaping_buffer.get());

        hb_shape(shaping_face.get(), shaping_buffer.get(), NULL, 0);

        unsigned int const len{hb_buffer_get_length(shaping_buffer.get())};
        hb_glyph_info_t const* const info{
            hb_buffer_get_glyph_infos(shaping_buffer.get(), NULL)};
        hb_glyph_position_t const* const pos{
            hb_buffer_get_glyph_positions(shaping_buffer.get(), NULL)};

        for (unsigned int i{}; i != len; i++)
        {
            hb_codepoint_t const gid{info[i].codepoint};
            unsigned int const cluster{info[i].cluster};
            double const x_advance{pos[i].x_advance / 64.};
            double const y_advance{pos[i].y_advance / 64.};
            double const x_offset{pos[i].x_offset / 64.};
            double const y_offset{pos[i].y_offset / 64.};

            char glyphname[32];
            hb_font_get_glyph_name(shaping_face.get(),
                gid,
                glyphname,
                sizeof(glyphname));

            spdlog::info("glyph='{}' cluster={} advance=({},{}) offset=({},{})",
                glyphname,
                cluster,
                x_advance,
                y_advance,
                x_offset,
                y_offset);
        }
    }
    else
    {
        std::terminate();
    }
}

void reshed::application_t::on_shutdown()
{
    vkDeviceWaitIdle(backend_->device().logical);

    imgui_.reset();

    backend_.reset();
}
