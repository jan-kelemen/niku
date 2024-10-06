#include <application.hpp>

#include <camera_controller.hpp>
#include <environment.hpp>
#include <materials.hpp>
#include <model_selector.hpp>
#include <pbr_renderer.hpp>
#include <postprocess_shader.hpp>
#include <render_graph.hpp>

#include <cppext_numeric.hpp>
#include <cppext_overloaded.hpp>
#include <cppext_pragma_warning.hpp>

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

DISABLE_WARNING_PUSH
DISABLE_WARNING_STRINGOP_OVERFLOW
#include <fmt/std.h> // IWYU pragma: keep
DISABLE_WARNING_POP

#include <imgui.h>

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_scancode.h>
#include <SDL2/SDL_video.h>

#include <spdlog/spdlog.h>

#include <tl/expected.hpp>

#include <volk.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

// IWYU pragma: no_include <fmt/base.h>
// IWYU pragma: no_include <string_view>

namespace
{
    [[nodiscard]] vkrndr::image_t create_color_image(
        vkrndr::backend_t const& backend)
    {
        return vkrndr::create_image_and_view(backend.device(),
            backend.extent(),
            1,
            backend.device().max_msaa_samples,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT);
    }
} // namespace

gltfviewer::application_t::application_t(bool const debug)
    : niku::application_t{niku::startup_params_t{
          .init_subsystems = {.video = true, .debug = debug},
          .title = "gltfviewer",
          .window_flags = static_cast<SDL_WindowFlags>(
              SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI),
          .centered = true,
          .width = 512,
          .height = 512}}
    , backend_{std::make_unique<vkrndr::backend_t>(*window(),
          vkrndr::render_settings_t{
              .preferred_swapchain_format = VK_FORMAT_R8G8B8A8_UNORM,
              .preferred_present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR,
          },
          debug)}
    , imgui_{std::make_unique<niku::imgui_layer_t>(*window(),
          backend_->context(),
          backend_->device(),
          backend_->swap_chain())}
    , color_image_{create_color_image(*backend_)}
    , environment_{std::make_unique<environment_t>(*backend_)}
    , materials_{std::make_unique<materials_t>(*backend_)}
    , render_graph_{std::make_unique<render_graph_t>(*backend_)}
    , pbr_renderer_{std::make_unique<pbr_renderer_t>(*backend_)}
    , postprocess_shader_{std::make_unique<postprocess_shader_t>(*backend_)}
    , camera_controller_{camera_, mouse_}
    , gltf_loader_{*backend_}
{
    auto const extent{backend_->extent()};

    camera_.set_aspect_ratio(
        cppext::as_fp(extent.width) / cppext::as_fp(extent.height));
    camera_.update();

    postprocess_shader_->update(gamma_, exposure_);
}

gltfviewer::application_t::~application_t() = default;

bool gltfviewer::application_t::handle_event(SDL_Event const& event)
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

void gltfviewer::application_t::update(float delta_time)
{
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

        materials_->load(*model);
        render_graph_->load(*model);
        pbr_renderer_->load(std::move(model).value(),
            environment_->descriptor_layout(),
            materials_->descriptor_layout(),
            render_graph_->descriptor_layout());

        spdlog::info("End loading: {}", model_path);
    }

    bool postprocess_updated{false};
    ImGui::Begin("Postprocess");
    postprocess_updated |= ImGui::SliderFloat("Gamma", &gamma_, 0.0f, 3.0f);
    postprocess_updated |=
        ImGui::SliderFloat("Exposure", &exposure_, 0.0f, 10.f);
    ImGui::End();

    if (postprocess_updated)
    {
        postprocess_shader_->update(gamma_, exposure_);
    }

    camera_controller_.update(delta_time);

    environment_->update(camera_);
}

bool gltfviewer::application_t::begin_frame()
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
        imgui_->begin_frame();
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

    vkrndr::wait_for_color_attachment_write(color_image_.image, command_buffer);

    if (VkPipelineLayout const layout{pbr_renderer_->pipeline_layout()})
    {
        environment_->bind_on(command_buffer,
            layout,
            VK_PIPELINE_BIND_POINT_GRAPHICS);

        materials_->bind_on(command_buffer,
            layout,
            VK_PIPELINE_BIND_POINT_GRAPHICS);

        render_graph_->bind_on(command_buffer,
            layout,
            VK_PIPELINE_BIND_POINT_GRAPHICS);
    }

    pbr_renderer_->draw(command_buffer, color_image_);

    vkrndr::transition_image(color_image_.image,
        command_buffer,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
        1);

    vkrndr::wait_for_color_attachment_write(target_image.image, command_buffer);

    postprocess_shader_->draw(command_buffer, color_image_, target_image);

    imgui_->render(command_buffer, target_image);

    vkrndr::transition_to_present_layout(target_image.image, command_buffer);

    backend_->draw();
}

void gltfviewer::application_t::end_frame()
{
    imgui_->end_frame();

    backend_->end_frame();
}

void gltfviewer::application_t::on_shutdown()
{
    vkDeviceWaitIdle(backend_->device().logical);

    postprocess_shader_.reset();

    pbr_renderer_.reset();

    render_graph_.reset();

    materials_.reset();

    environment_.reset();

    destroy(&backend_->device(), &color_image_);

    imgui_.reset();

    backend_.reset();
}

void gltfviewer::application_t::on_resize([[maybe_unused]] uint32_t width,
    [[maybe_unused]] uint32_t height)
{
    destroy(&backend_->device(), &color_image_);
    color_image_ = create_color_image(*backend_);

    camera_.set_aspect_ratio(cppext::as_fp(width) / cppext::as_fp(height));

    pbr_renderer_->resize(width, height);
}
