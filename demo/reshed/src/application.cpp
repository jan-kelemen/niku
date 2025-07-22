#include <application.hpp>

#include <text_editor.hpp>

#include <cppext_container.hpp>
#include <cppext_numeric.hpp>

#include <ngnwsi_application.hpp>
#include <ngnwsi_imgui_layer.hpp>
#include <ngnwsi_mouse.hpp>
#include <ngnwsi_render_window.hpp>
#include <ngnwsi_sdl_window.hpp>

#include <ngntxt_font_face.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_commands.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_instance.hpp>
#include <vkrndr_library_handle.hpp>
#include <vkrndr_render_pass.hpp>
#include <vkrndr_swapchain.hpp>
#include <vkrndr_synchronization.hpp>
#include <vkrndr_utility.hpp>

#include <imgui.h>

#include <volk.h>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_video.h>

#include <spdlog/spdlog.h>

#include <array>
#include <exception>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <system_error>
#include <utility>

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
    std::optional<vkrndr::queue_family_t> present_family;
    auto const create_result{vkrndr::initialize()
            .and_then(
                [this](vkrndr::library_handle_ptr_t&& library_handle)
                {
                    rendering_context_.library_handle =
                        std::move(library_handle);

                    std::vector<char const*> const instance_extensions{
                        ngnwsi::sdl_window_t::required_extensions()};

                    return vkrndr::create_instance({
                        .extensions = instance_extensions,
                        .application_name = "reshed",
                    });
                })
            .transform_error(
                [](std::error_code&& ec)
                {
                    spdlog::error("Instance creation failed with: {}",
                        ec.message());
                    return ec;
                })
            .and_then(
                [this, &present_family](
                    vkrndr::instance_ptr_t&& instance) mutable
                    -> std::expected<vkrndr::device_ptr_t, std::error_code>
                {
                    rendering_context_.instance = std::move(instance);
                    std::array const device_extensions{
                        VK_KHR_SWAPCHAIN_EXTENSION_NAME};

                    std::optional<vkrndr::physical_device_features_t> const
                        physical_device{pick_best_physical_device(
                            *rendering_context_.instance,
                            device_extensions,
                            render_window_->create_surface(
                                *rendering_context_.instance))};
                    if (!physical_device)
                    {
                        spdlog::error("No suitable physical device");
                        return std::unexpected{
                            make_error_code(std::errc::no_such_device)};
                    }

                    auto const queue_with_present{std::ranges::find_if(
                        physical_device->queue_families,
                        [](vkrndr::queue_family_t const& f)
                        {
                            return f.supports_present &&
                                vkrndr::supports_flags(f.properties.queueFlags,
                                    VK_QUEUE_GRAPHICS_BIT);
                        })};
                    if (queue_with_present ==
                        std::cend(physical_device->queue_families))
                    {
                        spdlog::error("No present queue");
                        return std::unexpected{
                            make_error_code(std::errc::not_supported)};
                    }
                    present_family = *queue_with_present;

                    return create_device(*rendering_context_.instance,
                        device_extensions,
                        *physical_device,
                        cppext::as_span(*queue_with_present));
                })
            .transform_error(
                [](std::error_code&& ec)
                {
                    spdlog::error("Device creation failed with: {}",
                        ec.message());
                    return ec;
                })
            .and_then(
                [this, &present_family](vkrndr::device_ptr_t&& device)
                {
                    rendering_context_.device = std::move(device);

                    backend_ =
                        std::make_unique<vkrndr::backend_t>(rendering_context_,
                            2);

                    vkrndr::execution_port_t& present_queue{
                        *std::ranges::find_if(
                            rendering_context_.device->execution_ports,
                            [&present_family](
                                vkrndr::execution_port_t const& port)
                            {
                                return port.queue_family() ==
                                    present_family->index;
                            })};

                    vkrndr::swapchain_t* const swapchain{
                        render_window_->create_swapchain(
                            *rendering_context_.device,
                            {
                                .preffered_format = VK_FORMAT_R8G8B8A8_UNORM,
                                .image_flags =
                                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                    VK_IMAGE_USAGE_STORAGE_BIT |
                                    VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                .preferred_present_mode =
                                    VK_PRESENT_MODE_FIFO_KHR,
                                .present_queue = &present_queue,
                            })};

                    imgui_ = std::make_unique<ngnwsi::imgui_layer_t>(
                        render_window_->platform_window(),
                        *rendering_context_.instance,
                        *rendering_context_.device,
                        *swapchain,
                        present_queue);

                    editor_ = std::make_unique<text_editor_t>(*backend_,
                        swapchain->image_format());

                    return std::expected<std::void_t<>, std::error_code>{};
                })};
    if (!create_result)
    {
        throw std::system_error{create_result.error()};
    }
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

            vkDeviceWaitIdle(backend_->device());

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

    vkDeviceWaitIdle(backend_->device());

    editor_.reset();

    imgui_.reset();

    if (render_window_)
    {
        render_window_->destroy_swapchain();
        render_window_->destroy_surface(backend_->instance().handle);
        render_window_.reset();
    }

    backend_.reset();

    rendering_context_.device.reset();
    rendering_context_.instance.reset();
    rendering_context_.library_handle.reset();
}

void reshed::application_t::on_resize(uint32_t width, uint32_t height)
{
    vkDeviceWaitIdle(backend_->device());

    render_window_->swapchain().recreate();

    editor_->resize(width, height);
}
