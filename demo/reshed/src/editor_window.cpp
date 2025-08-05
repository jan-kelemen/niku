#include <editor_window.hpp>

#include <text_editor.hpp>

#include <cppext_container.hpp>
#include <cppext_numeric.hpp>

#include <ngntxt_font_face.hpp>

#include <ngnwsi_imgui_layer.hpp>
#include <ngnwsi_render_window.hpp>
#include <ngnwsi_sdl_window.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_commands.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_execution_port.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_instance.hpp>
#include <vkrndr_render_pass.hpp>
#include <vkrndr_swapchain.hpp>
#include <vkrndr_synchronization.hpp>
#include <vkrndr_utility.hpp>

#include <imgui.h>

#include <SDL3/SDL_events.h>
#include <SDL_scancode.h>
#include <SDL_video.h>

#include <volk.h>

#include <algorithm>
#include <optional>
#include <span>
#include <utility>

// IWYU pragma: no_include <boost/smart_ptr/intrusive_ref_counter.hpp>
// IWYU pragma: no_include <boost/smart_ptr/intrusive_ptr.hpp>
// IWYU pragma: no_include <functional>
// IWYU pragma: no_include <vector>

reshed::editor_window_t::editor_window_t(vkrndr::rendering_context_t context,
    uint32_t const frames_in_flight)
    : editor_window_t{std::make_unique<ngnwsi::render_window_t>("reshed",
                          SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY,
                          512,
                          512),
          std::move(context),
          frames_in_flight}
{
}

reshed::editor_window_t::editor_window_t(
    std::unique_ptr<ngnwsi::render_window_t>&& window,
    vkrndr::rendering_context_t context,
    uint32_t const frames_in_flight)
    : instance_{context.instance}
    , render_window_(std::move(window))
{
    static_cast<void>(render_window_->create_surface(*instance_));

    vkrndr::execution_port_t& present_queue{
        *std::ranges::find_if(context.device->execution_ports,
            &vkrndr::execution_port_t::has_present)};

    vkrndr::swapchain_t* const swapchain{
        render_window_->create_swapchain(*context.device,
            {
                .preferred_format = VK_FORMAT_R8G8B8A8_UNORM,
                .image_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                    VK_IMAGE_USAGE_STORAGE_BIT |
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                .preferred_present_mode = VK_PRESENT_MODE_FIFO_KHR,
                .frames_in_flight = frames_in_flight,
                .present_queue = &present_queue,
            })};

    imgui_layer_ = std::make_unique<ngnwsi::imgui_layer_t>(
        render_window_->platform_window(),
        *context.instance,
        *context.device,
        *swapchain,
        present_queue);

    backend_ = std::make_unique<vkrndr::backend_t>(std::move(context),
        frames_in_flight);

    editor_ =
        std::make_unique<text_editor_t>(*backend_, swapchain->image_format());

    text_input_guard_ = std::make_unique<ngnwsi::sdl_text_input_guard_t>(
        render_window_->platform_window());

    auto const& [width, height] = swapchain->extent();
    editor_->resize(width, height);
}

reshed::editor_window_t::editor_window_t(editor_window_t&&) noexcept = default;

reshed::editor_window_t::~editor_window_t()
{
    if (editor_)
    {
        vkDeviceWaitIdle(backend_->device());

        editor_.reset();

        text_input_guard_.reset();

        imgui_layer_.reset();

        render_window_->destroy_swapchain();
        render_window_->destroy_surface(*instance_);
        render_window_.reset();

        backend_.reset();
    }
}

bool reshed::editor_window_t::is_focused() const noexcept
{
    return render_window_->platform_window().is_focused();
}

uint32_t reshed::editor_window_t::window_id() const noexcept
{
    return render_window_->platform_window().window_id();
}

void reshed::editor_window_t::use_font(ngntxt::font_face_ptr_t font)
{
    editor_->change_font(std::move(font));
}

bool reshed::editor_window_t::handle_event(SDL_Event const& event)
{
    [[maybe_unused]] auto imgui_handled{imgui_layer_->handle_event(event)};

    if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
    {
        vkDeviceWaitIdle(backend_->device());

        editor_.reset();

        text_input_guard_.reset();

        imgui_layer_.reset();

        render_window_->destroy_swapchain();
        render_window_->destroy_surface(*instance_);
        render_window_.reset();

        backend_.reset();
    }

    if (event.type == SDL_EVENT_WINDOW_RESIZED)
    {
        uint32_t const width{cppext::narrow<uint32_t>(event.window.data1)};
        uint32_t const height{cppext::narrow<uint32_t>(
            cppext::narrow<uint32_t>(event.window.data2))};

        render_window_->resize(width, height);
        editor_->resize(width, height);
    }

    if (event.type == SDL_EVENT_KEY_DOWN &&
        event.key.scancode == SDL_SCANCODE_F4)
    {
        imgui_layer_->set_enabled(!imgui_layer_->enabled());
        return true;
    }

    if (event.type == SDL_EVENT_TEXT_INPUT ||
        event.type == SDL_EVENT_KEY_UP ||
        event.type == SDL_EVENT_KEY_DOWN)
    {
        editor_->handle_event(event);
        return true;
    }

    return false;
}

void reshed::editor_window_t::draw()
{
    std::optional<vkrndr::image_t> const target_image{
        render_window_->acquire_next_image()};
    if (!target_image)
    {
        return;
    }

    backend_->begin_frame();

    imgui_layer_->begin_frame();

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

    imgui_layer_->render(command_buffer, *target_image);

    vkrndr::transition_to_present_layout(target_image->image, command_buffer);

    render_window_->present(backend_->present_buffers());

    backend_->end_frame();
}

reshed::editor_window_t& reshed::editor_window_t::operator=(
    editor_window_t&&) noexcept = default;
