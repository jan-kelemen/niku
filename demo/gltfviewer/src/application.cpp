#include <application.hpp>

#include <camera_controller.hpp>
#include <environment.hpp>
#include <materials.hpp>
#include <model_selector.hpp>
#include <pbr_shader.hpp>
#include <postprocess_shader.hpp>
#include <pyramid_blur.hpp>
#include <render_graph.hpp>
#include <resolve_shader.hpp>
#include <weighted_blend_shader.hpp>
#include <weighted_oit_shader.hpp>

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

// IWYU pragma: no_include <fmt/base.h>
// IWYU pragma: no_include <fmt/format.h>
// IWYU pragma: no_include <expected>
// IWYU pragma: no_include <optional>
// IWYU pragma: no_include <string_view>
// IWYU pragma: no_include <system_error>

namespace
{
    struct [[nodiscard]] push_constants_t final
    {
        // cppcheck-suppress unusedStructMember
        uint32_t debug;
        float ibl_factor;
    };

    constexpr std::array debug_options{"None",
        "Albedo",
        "Normals",
        "Occlusion",
        "Emissive",
        "Metallic",
        "Roughness",
        "UV"};

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

    [[nodiscard]] vkrndr::image_t create_resolve_image(
        vkrndr::backend_t const& backend)
    {
        return vkrndr::create_image_and_view(backend.device(),
            backend.extent(),
            1,
            VK_SAMPLE_COUNT_1_BIT,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT);
    }

    [[nodiscard]] vkrndr::image_t create_depth_buffer(
        vkrndr::backend_t const& backend)
    {
        return vkrndr::create_depth_buffer(backend.device(),
            backend.extent(),
            false,
            backend.device().max_msaa_samples);
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
    , environment_{std::make_unique<environment_t>(*backend_)}
    , materials_{std::make_unique<materials_t>(*backend_)}
    , render_graph_{std::make_unique<render_graph_t>(*backend_)}
    , pbr_shader_{std::make_unique<pbr_shader_t>(*backend_)}
    , weighted_oit_shader_{std::make_unique<weighted_oit_shader_t>(*backend_)}
    , resolve_shader_{std::make_unique<resolve_shader_t>(*backend_)}
    , pyramid_blur_{std::make_unique<pyramid_blur_t>(*backend_)}
    , weighted_blend_shader_{std::make_unique<weighted_blend_shader_t>(
          *backend_)}
    , postprocess_shader_{std::make_unique<postprocess_shader_t>(*backend_)}
    , camera_controller_{camera_, mouse_}
    , gltf_loader_{*backend_}
{
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
        render_graph_->load(std::move(model).value());
        pbr_shader_->load(*render_graph_,
            environment_->descriptor_layout(),
            materials_->descriptor_layout(),
            depth_buffer_.format);
        weighted_oit_shader_->load(*render_graph_,
            environment_->descriptor_layout(),
            materials_->descriptor_layout(),
            depth_buffer_.format);

        spdlog::info("End loading: {}", model_path);
    }

    ImGui::Begin("Rendering");

    if (ImGui::BeginCombo("PBR Equation", debug_options[debug_], 0))
    {
        uint32_t i{};
        for (auto const& option : debug_options)
        {
            auto const selected{debug_ == i};

            if (ImGui::Selectable(option, selected))
            {
                debug_ = i;
            }

            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }

            ++i;
        }
        ImGui::EndCombo();
    }

    ImGui::SliderFloat("IBL Factor", &ibl_factor_, 0.0f, 9.0f);
    ImGui::Checkbox("Color Conversion", &color_conversion_);
    ImGui::Checkbox("Tone Mapping", &tone_mapping_);
    ImGui::End();

    camera_controller_.update(delta_time);

    environment_->update(camera_);
}

bool gltfviewer::application_t::begin_frame()
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

void gltfviewer::application_t::draw()
{
    ImGui::ShowMetricsWindow();

    camera_controller_.draw_imgui();

    auto target_image{backend_->swapchain_image()};

    VkCommandBuffer command_buffer{backend_->request_command_buffer()};

    vkrndr::wait_for_color_attachment_write(color_image_.image, command_buffer);

    VkViewport const viewport{.x = 0.0f,
        .y = 0.0f,
        .width = cppext::as_fp(target_image.extent.width),
        .height = cppext::as_fp(target_image.extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f};
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D const scissor{{0, 0}, target_image.extent};
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    push_constants_t const pc{.debug = debug_, .ibl_factor = ibl_factor_};

    if (render_graph_->model().vertex_count)
    {
        VkPipelineLayout const pbr_layout{pbr_shader_->pipeline_layout()};
        environment_->bind_on(command_buffer,
            pbr_layout,
            VK_PIPELINE_BIND_POINT_GRAPHICS);

        materials_->bind_on(command_buffer,
            pbr_layout,
            VK_PIPELINE_BIND_POINT_GRAPHICS);

        render_graph_->bind_on(command_buffer,
            pbr_layout,
            VK_PIPELINE_BIND_POINT_GRAPHICS);

        vkCmdPushConstants(command_buffer,
            pbr_layout,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            8,
            &pc);
    }

    pbr_shader_->draw(*render_graph_,
        command_buffer,
        color_image_,
        depth_buffer_);

    environment_->draw_skybox(command_buffer, color_image_, depth_buffer_);

    if (render_graph_->model().vertex_count)
    {
        VkPipelineLayout const oit_layout{
            weighted_oit_shader_->pipeline_layout()};
        environment_->bind_on(command_buffer,
            oit_layout,
            VK_PIPELINE_BIND_POINT_GRAPHICS);

        materials_->bind_on(command_buffer,
            oit_layout,
            VK_PIPELINE_BIND_POINT_GRAPHICS);

        render_graph_->bind_on(command_buffer,
            oit_layout,
            VK_PIPELINE_BIND_POINT_GRAPHICS);

        vkCmdPushConstants(command_buffer,
            oit_layout,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            8,
            &pc);

        weighted_oit_shader_->draw(*render_graph_,
            command_buffer,
            color_image_,
            depth_buffer_);
    }

    {
        [[maybe_unused]] vkrndr::command_buffer_scope_t const
            postprocess_cb_scope{command_buffer, "Postprocess"};

        vkrndr::transition_image(color_image_.image,
            command_buffer,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            1);

        vkrndr::transition_image(resolve_image_.image,
            command_buffer,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            1);

        auto const& blur_image{pyramid_blur_->source_image()};
        vkrndr::transition_image(blur_image.image,
            command_buffer,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            1);

        resolve_shader_->draw(command_buffer,
            color_image_,
            resolve_image_,
            blur_image);

        pyramid_blur_->draw(command_buffer);

        vkrndr::transition_image(blur_image.image,
            command_buffer,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
            1);

        vkrndr::transition_image(resolve_image_.image,
            command_buffer,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            1);

        weighted_blend_shader_->draw(command_buffer,
            resolve_image_,
            blur_image);

        vkrndr::transition_image(resolve_image_.image,
            command_buffer,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            1);

        vkrndr::transition_image(target_image.image,
            command_buffer,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_NONE,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            1);

        postprocess_shader_->draw(color_conversion_,
            tone_mapping_,
            command_buffer,
            resolve_image_,
            target_image);

        vkrndr::transition_image(target_image.image,
            command_buffer,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT |
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            1);
    }

    imgui_->render(command_buffer, target_image);

    vkrndr::transition_to_present_layout(target_image.image, command_buffer);

    backend_->draw();
}

void gltfviewer::application_t::end_frame()
{
    imgui_->end_frame();

    backend_->end_frame();
}

void gltfviewer::application_t::on_startup()
{
    auto const extent{backend_->extent()};
    on_resize(extent.width, extent.height);

    environment_->load_skybox("aviation_museum_4k.hdr", depth_buffer_.format);
}

void gltfviewer::application_t::on_shutdown()
{
    vkDeviceWaitIdle(backend_->device().logical);

    postprocess_shader_.reset();

    weighted_blend_shader_.reset();

    pyramid_blur_.reset();

    resolve_shader_.reset();

    weighted_oit_shader_.reset();

    pbr_shader_.reset();

    render_graph_.reset();

    materials_.reset();

    environment_.reset();

    destroy(&backend_->device(), &resolve_image_);

    destroy(&backend_->device(), &depth_buffer_);

    destroy(&backend_->device(), &color_image_);

    imgui_.reset();

    backend_.reset();
}

void gltfviewer::application_t::on_resize(uint32_t const width,
    uint32_t const height)
{
    destroy(&backend_->device(), &color_image_);
    color_image_ = create_color_image(*backend_);
    object_name(backend_->device(), color_image_, "Offscreen Image");

    destroy(&backend_->device(), &depth_buffer_);
    depth_buffer_ = create_depth_buffer(*backend_);
    object_name(backend_->device(), depth_buffer_, "Depth Buffer");

    destroy(&backend_->device(), &resolve_image_);
    resolve_image_ = create_resolve_image(*backend_);
    object_name(backend_->device(), resolve_image_, "Resolve Image");

    weighted_oit_shader_->resize(width, height);

    pyramid_blur_->resize(width, height);

    camera_.set_aspect_ratio(cppext::as_fp(width) / cppext::as_fp(height));
}
