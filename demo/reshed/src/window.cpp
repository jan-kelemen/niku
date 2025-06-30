#include "ngntxt_font_face.hpp"
#include <memory>
#include <window.hpp>

#include <text_editor.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_command_pool.hpp>
#include <vkrndr_commands.hpp>
#include <vkrndr_render_pass.hpp>
#include <vkrndr_utility.hpp>

#include <imgui.h>

#include <SDL3/SDL_events.h>

#include <spdlog/spdlog.h>

reshed::window_t::window_t()
    : ngnwsi::window_t("reshed",
          SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY,
          512,
          512)
    , text_input_{native_window()}
{
}

void reshed::window_t::init_rendering(vkrndr::backend_t& backend)
{
    vkrndr::swap_chain_t* const swapchain{create_swapchain(backend.device(),
        vkrndr::render_settings_t{
            .required_extensions = ngnwsi::sdl_window_t::required_extensions(),
            .preferred_swapchain_format = VK_FORMAT_R8G8B8A8_UNORM,
            .swapchain_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                VK_IMAGE_USAGE_STORAGE_BIT |
                VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .preferred_present_mode = VK_PRESENT_MODE_FIFO_KHR,
        })};

    imgui_ = std::make_unique<ngnwsi::imgui_layer_t>(native_window(),
        backend.instance(),
        backend.device(),
        *swapchain);

    editor_ = std::make_unique<text_editor_t>(backend, *this);

    frame_data_ =
        cppext::cycled_buffer_t<frame_data_t>{backend.frames_in_flight(),
            backend.frames_in_flight()};

    for (frame_data_t& fd : cppext::as_span(frame_data_))
    {
        fd.present_command_pool =
            std::make_unique<vkrndr::command_pool_t>(*device_,
                device_->present_queue->queue_family());

        fd.present_command_pool->allocate_command_buffers(true,
            1,
            cppext::as_span(fd.present_command_buffer));
    };
}

void reshed::window_t::set_font(ngntxt::font_face_ptr_t font_face)
{
    editor_->change_font(std::move(font_face));
}

void reshed::window_t::handle_event(SDL_Event const& event)
{
    spdlog::info("event");

    [[maybe_unused]] auto imgui_handled{imgui_->handle_event(event)};

    if (event.type == SDL_EVENT_KEY_DOWN &&
        event.key.scancode == SDL_SCANCODE_F4)
    {
        imgui_->set_enabled(!imgui_->enabled());
    }

    if (event.type == SDL_EVENT_TEXT_INPUT ||
        event.type == SDL_EVENT_KEY_UP ||
        event.type == SDL_EVENT_KEY_DOWN)
    {
        editor_->handle_event(event);
    }
}

bool reshed::window_t::begin_frame()
{
    should_render_ = acquire_image();

    if (should_render_)
    {
        imgui_->begin_frame();

        frame_data_.cycle(
            [](auto const&, frame_data_t& next)
            {
                next.present_command_pool->reset();

                VkCommandBufferBeginInfo const begin_info{
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

                vkrndr::check_result(
                    vkBeginCommandBuffer(next.present_command_buffer,
                        &begin_info));
            });
    }

    return should_render_;
}

void reshed::window_t::render()
{
    if (!std::exchange(should_render_, false))
    {
        return;
    }

    auto target_image{swapchain_image()};

    VkCommandBuffer command_buffer{frame_data_->present_command_buffer};

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

    vkEndCommandBuffer(command_buffer);

    present(cppext::as_span(command_buffer));
}

void reshed::window_t::debug_draw()
{
    if (!should_render_)
    {
        return;
    }

    ImGui::ShowMetricsWindow();

    editor_->debug_draw();
}

void reshed::window_t::end_frame() { imgui_->end_frame(); }
