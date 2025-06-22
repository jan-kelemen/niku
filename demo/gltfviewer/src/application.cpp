#include <application.hpp>

#include <camera_controller.hpp>
#include <depth_pass_shader.hpp>
#include <environment.hpp>
#include <materials.hpp>
#include <model_selector.hpp>
#include <pbr_shader.hpp>
#include <postprocess_shader.hpp>
#include <pyramid_blur.hpp>
#include <resolve_shader.hpp>
#include <scene_graph.hpp>
#include <shadow_map.hpp>
#include <weighted_blend_shader.hpp>
#include <weighted_oit_shader.hpp>

#include <cppext_container.hpp>
#include <cppext_numeric.hpp>
#include <cppext_overloaded.hpp>
#include <cppext_pragma_warning.hpp>

#include <ngnast_gltf_loader.hpp>

#include <ngngfx_aircraft_camera.hpp>
#include <ngngfx_perspective_projection.hpp>

#include <ngnwsi_application.hpp>
#include <ngnwsi_imgui_layer.hpp>
#include <ngnwsi_mouse.hpp>
#include <ngnwsi_sdl_window.hpp> // IWYU pragma: keep

#include <vkrndr_backend.hpp>
#include <vkrndr_commands.hpp>
#include <vkrndr_debug_utils.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_formats.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_library.hpp>
#include <vkrndr_render_pass.hpp>
#include <vkrndr_render_settings.hpp>
#include <vkrndr_swap_chain.hpp>
#include <vkrndr_synchronization.hpp>
#include <vkrndr_utility.hpp>

DISABLE_WARNING_PUSH
DISABLE_WARNING_STRINGOP_OVERFLOW
#include <fmt/std.h> // IWYU pragma: keep
DISABLE_WARNING_POP

#include <imgui.h>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_video.h>

#include <spdlog/spdlog.h>

#include <vma_impl.hpp>

#include <volk.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iterator>
#include <memory>
#include <span>
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

    constexpr std::array present_modes{
        std::make_pair(VK_PRESENT_MODE_IMMEDIATE_KHR, "Immediate"),
        std::make_pair(VK_PRESENT_MODE_MAILBOX_KHR, "Mailbox"),
        std::make_pair(VK_PRESENT_MODE_FIFO_KHR, "FIFO"),
        std::make_pair(VK_PRESENT_MODE_FIFO_RELAXED_KHR, "FIFO Relaxed"),
        std::make_pair(VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR,
            "Shared Demand Refresh"),
        std::make_pair(VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR,
            "Shared Continous Refresh"),
        std::make_pair(VK_PRESENT_MODE_FIFO_LATEST_READY_EXT,
            "FIFO Latest Ready")};

    [[nodiscard]] vkrndr::image_t create_color_image(
        vkrndr::backend_t const& backend)
    {
        return vkrndr::create_image_and_view(backend.device(),
            vkrndr::image_2d_create_info_t{
                .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                .extent = backend.extent(),
                .samples = backend.device().max_msaa_samples,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                    VK_IMAGE_USAGE_SAMPLED_BIT,
                .allocation_flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                .required_memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
            VK_IMAGE_ASPECT_COLOR_BIT);
    }

    [[nodiscard]] vkrndr::image_t create_resolve_image(
        vkrndr::backend_t const& backend)
    {
        return vkrndr::create_image_and_view(backend.device(),
            vkrndr::image_2d_create_info_t{
                .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                .extent = backend.extent(),
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage =
                    VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                .allocation_flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                .required_memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
            VK_IMAGE_ASPECT_COLOR_BIT);
    }

    [[nodiscard]] vkrndr::image_t create_depth_buffer(
        vkrndr::backend_t const& backend)
    {
        auto const& formats{vkrndr::find_supported_depth_stencil_formats(
            backend.device().physical,
            true,
            false)};
        vkrndr::image_2d_create_info_t create_info{.extent = backend.extent(),
            .samples = backend.device().max_msaa_samples,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            .allocation_flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            .required_memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT};

        auto const opt_it{std::ranges::find_if(formats,
            [](auto const& f)
            {
                return f.properties.optimalTilingFeatures &
                    VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
            })};
        if (opt_it != std::cend(formats))
        {
            create_info.format = opt_it->format;
            create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
            return create_image_and_view(backend.device(),
                create_info,
                VK_IMAGE_ASPECT_DEPTH_BIT);
        }

        auto const lin_it{std::ranges::find_if(formats,
            [](auto const& f)
            {
                return f.properties.linearTilingFeatures &
                    VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
            })};
        if (lin_it != std::cend(formats))
        {
            create_info.format = lin_it->format;
            create_info.tiling = VK_IMAGE_TILING_LINEAR;
            return create_image_and_view(backend.device(),
                create_info,
                VK_IMAGE_ASPECT_DEPTH_BIT);
        }

        std::terminate();
    }
} // namespace

gltfviewer::application_t::application_t(bool const debug)
    : ngnwsi::application_t{ngnwsi::startup_params_t{
          .init_subsystems = {.video = true, .debug = debug},
          .title = "gltfviewer",
          .window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY,
          .width = 512,
          .height = 512}}
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
    , environment_{std::make_unique<environment_t>(*backend_)}
    , materials_{std::make_unique<materials_t>(*backend_)}
    , scene_graph_{std::make_unique<scene_graph_t>(*backend_)}
    , depth_pass_shader_{std::make_unique<depth_pass_shader_t>(*backend_)}
    , shadow_map_{std::make_unique<shadow_map_t>(*backend_)}
    , pbr_shader_{std::make_unique<pbr_shader_t>(*backend_)}
    , weighted_oit_shader_{std::make_unique<weighted_oit_shader_t>(*backend_)}
    , resolve_shader_{std::make_unique<resolve_shader_t>(*backend_)}
    , pyramid_blur_{std::make_unique<pyramid_blur_t>(*backend_)}
    , weighted_blend_shader_{std::make_unique<weighted_blend_shader_t>(
          *backend_)}
    , postprocess_shader_{std::make_unique<postprocess_shader_t>(*backend_)}
    , camera_controller_{camera_, mouse_}
{
}

gltfviewer::application_t::~application_t() = default;

bool gltfviewer::application_t::handle_event(SDL_Event const& event)
{
    camera_controller_.handle_event(event);

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

        vkDeviceWaitIdle(backend_->device().logical);

        materials_->load(*model);
        scene_graph_->load(std::move(model).value());
        depth_pass_shader_->load(*scene_graph_,
            environment_->descriptor_layout(),
            materials_->descriptor_layout(),
            depth_buffer_.format);
        shadow_map_->load(*scene_graph_,
            environment_->descriptor_layout(),
            materials_->descriptor_layout(),
            depth_buffer_.format);
        pbr_shader_->load(*scene_graph_,
            environment_->descriptor_layout(),
            materials_->descriptor_layout(),
            shadow_map_->descriptor_layout(),
            depth_buffer_.format);
        weighted_oit_shader_->load(*scene_graph_,
            environment_->descriptor_layout(),
            materials_->descriptor_layout(),
            shadow_map_->descriptor_layout(),
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
    if (int v{cppext::narrow<int>(blur_levels_)};
        ImGui::SliderInt("Blur Levels",
            &v,
            1,
            cppext::narrow<int>(pyramid_blur_->source_image().mip_levels) - 1))
    {
        blur_levels_ = static_cast<uint32_t>(v);
    }

    ImGui::SliderFloat("Bloom Strength", &bloom_strength_, 0.0f, 1.0f);
    ImGui::Checkbox("Color Conversion", &color_conversion_);
    ImGui::Checkbox("Tone Mapping", &tone_mapping_);
    ImGui::Checkbox("OIT", &transparent_);

    auto const& available_modes{
        backend_->swap_chain().available_present_modes()};
    auto const current_mode{backend_->swap_chain().present_mode()};
    // NOLINTBEGIN(readability-qualified-auto)
    auto const present_mode_it{std::ranges::find(present_modes,
        current_mode,
        &std::pair<VkPresentModeKHR, char const*>::first)};
    if (present_mode_it != std::end(present_modes))
    {
        if (ImGui::BeginCombo("Present Mode", present_mode_it->second))
        {
            for (auto const& option : available_modes)
            {
                auto const selected{option == current_mode};
                auto const other_it{std::ranges::find(present_modes,
                    option,
                    &std::pair<VkPresentModeKHR, char const*>::first)};
                if (other_it == std::end(present_modes))
                {
                    continue;
                }

                if (ImGui::Selectable(other_it->second, selected))
                {
                    if (!backend_->swap_chain().change_present_mode(option))
                    {
                        spdlog::warn("Unable to switch to present mode {}",
                            other_it->second);
                    }
                }

                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }
    // NOLINTEND(readability-qualified-auto)

    ImGui::End();

    camera_controller_.update(delta_time);
    projection_.update(camera_.view_matrix());

    environment_->update(camera_, projection_);
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

    {
        auto const barrier{vkrndr::to_layout(
            vkrndr::with_access(
                vkrndr::on_stage(vkrndr::image_barrier(color_image_),
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT),
                VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT),
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)};

        vkrndr::wait_for(command_buffer, {}, {}, cppext::as_span(barrier));
    }

    VkViewport const viewport{.x = 0.0f,
        .y = 0.0f,
        .width = cppext::as_fp(target_image.extent.width),
        .height = cppext::as_fp(target_image.extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f};
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D const scissor{{0, 0}, vkrndr::to_2d_extent(target_image.extent)};
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    push_constants_t const pc{.debug = debug_, .ibl_factor = ibl_factor_};

    if (!scene_graph_->empty())
    {
        {
            vkrndr::render_pass_t depth_render_pass;
            depth_render_pass.with_depth_attachment(VK_ATTACHMENT_LOAD_OP_CLEAR,
                VK_ATTACHMENT_STORE_OP_STORE,
                depth_buffer_.view,
                VkClearValue{.depthStencil = {1.0f, 0}});

            VkPipelineLayout const layout{
                depth_pass_shader_->pipeline_layout()};
            environment_->bind_on(command_buffer,
                layout,
                VK_PIPELINE_BIND_POINT_GRAPHICS);

            materials_->bind_on(command_buffer,
                layout,
                VK_PIPELINE_BIND_POINT_GRAPHICS);

            scene_graph_->bind_on(command_buffer,
                layout,
                VK_PIPELINE_BIND_POINT_GRAPHICS);

            vkCmdPushConstants(command_buffer,
                layout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                8,
                &pc);

            {
                [[maybe_unused]] auto guard{
                    depth_render_pass.begin(command_buffer,
                        {{0, 0}, vkrndr::to_2d_extent(depth_buffer_.extent)})};

                depth_pass_shader_->draw(*scene_graph_, command_buffer);
            }

            auto const barrier{vkrndr::with_access(
                vkrndr::on_stage(vkrndr::image_barrier(depth_buffer_,
                                     VK_IMAGE_ASPECT_DEPTH_BIT),
                    VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                    VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT),
                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT)};

            vkrndr::wait_for(command_buffer, {}, {}, cppext::as_span(barrier));
        }

        if (environment_->has_directional_lights())
        {
            VkPipelineLayout const layout{shadow_map_->pipeline_layout()};
            environment_->bind_on(command_buffer,
                layout,
                VK_PIPELINE_BIND_POINT_GRAPHICS);

            materials_->bind_on(command_buffer,
                layout,
                VK_PIPELINE_BIND_POINT_GRAPHICS);

            scene_graph_->bind_on(command_buffer,
                layout,
                VK_PIPELINE_BIND_POINT_GRAPHICS);

            vkCmdPushConstants(command_buffer,
                layout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                8,
                &pc);

            shadow_map_->draw(*scene_graph_, command_buffer);
        }

        {
            vkrndr::render_pass_t color_render_pass;
            color_render_pass.with_color_attachment(VK_ATTACHMENT_LOAD_OP_CLEAR,
                VK_ATTACHMENT_STORE_OP_STORE,
                color_image_.view,
                VkClearValue{.color = {{0.0f, 0.0f, 0.0f, 1.0f}}});
            color_render_pass.with_depth_attachment(VK_ATTACHMENT_LOAD_OP_LOAD,
                VK_ATTACHMENT_STORE_OP_STORE,
                depth_buffer_.view);

            VkPipelineLayout const layout{pbr_shader_->pipeline_layout()};
            environment_->bind_on(command_buffer,
                layout,
                VK_PIPELINE_BIND_POINT_GRAPHICS);

            materials_->bind_on(command_buffer,
                layout,
                VK_PIPELINE_BIND_POINT_GRAPHICS);

            scene_graph_->bind_on(command_buffer,
                layout,
                VK_PIPELINE_BIND_POINT_GRAPHICS);

            shadow_map_->bind_on(command_buffer,
                layout,
                VK_PIPELINE_BIND_POINT_GRAPHICS);

            vkCmdPushConstants(command_buffer,
                layout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                8,
                &pc);

            {
                [[maybe_unused]] auto guard{
                    color_render_pass.begin(command_buffer,
                        {{0, 0}, vkrndr::to_2d_extent(color_image_.extent)})};

                pbr_shader_->draw(*scene_graph_, command_buffer);

                environment_->draw_skybox(command_buffer);
            }
        }
    }
    else
    {
        vkrndr::render_pass_t color_render_pass;
        color_render_pass.with_color_attachment(VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            color_image_.view,
            VkClearValue{.color = {{0.0f, 0.0f, 0.0f, 1.0f}}});
        color_render_pass.with_depth_attachment(VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            depth_buffer_.view,
            VkClearValue{.depthStencil = {1.0f, 0}});

        {
            [[maybe_unused]] auto guard{color_render_pass.begin(command_buffer,
                {{0, 0}, vkrndr::to_2d_extent(color_image_.extent)})};

            environment_->draw_skybox(command_buffer);
        }
    }

    {
        std::array const barriers{
            vkrndr::with_access(
                vkrndr::on_stage(vkrndr::image_barrier(color_image_),
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT),
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT),
            vkrndr::with_access(
                vkrndr::on_stage(vkrndr::image_barrier(depth_buffer_,
                                     VK_IMAGE_ASPECT_DEPTH_BIT),
                    VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                    VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT),
                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT)};

        vkrndr::wait_for(command_buffer, {}, {}, barriers);
    }

    if (transparent_ && !scene_graph_->empty())
    {
        VkPipelineLayout const oit_layout{
            weighted_oit_shader_->pipeline_layout()};
        environment_->bind_on(command_buffer,
            oit_layout,
            VK_PIPELINE_BIND_POINT_GRAPHICS);

        materials_->bind_on(command_buffer,
            oit_layout,
            VK_PIPELINE_BIND_POINT_GRAPHICS);

        scene_graph_->bind_on(command_buffer,
            oit_layout,
            VK_PIPELINE_BIND_POINT_GRAPHICS);

        shadow_map_->bind_on(command_buffer,
            oit_layout,
            VK_PIPELINE_BIND_POINT_GRAPHICS);

        vkCmdPushConstants(command_buffer,
            oit_layout,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            8,
            &pc);

        weighted_oit_shader_->draw(*scene_graph_,
            command_buffer,
            color_image_,
            depth_buffer_);
    }

    {
        VKRNDR_IF_DEBUG_UTILS(
            [[maybe_unused]] vkrndr::command_buffer_scope_t const
                postprocess_cb_scope{command_buffer, "Postprocess"});

        auto const& blur_image{pyramid_blur_->source_image()};
        {
            std::array const barriers{
                vkrndr::with_layout(
                    vkrndr::with_access(
                        vkrndr::on_stage(vkrndr::image_barrier(color_image_),
                            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT),
                        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                        VK_ACCESS_2_SHADER_SAMPLED_READ_BIT),
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
                vkrndr::to_layout(
                    vkrndr::with_access(
                        vkrndr::on_stage(vkrndr::image_barrier(resolve_image_),
                            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT),
                        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT),
                    VK_IMAGE_LAYOUT_GENERAL),
                vkrndr::to_layout(
                    vkrndr::with_access(
                        vkrndr::on_stage(vkrndr::image_barrier(blur_image),
                            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT),
                        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT),
                    VK_IMAGE_LAYOUT_GENERAL)};

            vkrndr::wait_for(command_buffer, {}, {}, barriers);
        }

        resolve_shader_->draw(command_buffer,
            color_image_,
            resolve_image_,
            blur_image);

        pyramid_blur_->draw(blur_levels_, command_buffer);

        {
            std::array const barriers{
                vkrndr::with_access(
                    vkrndr::on_stage(vkrndr::image_barrier(resolve_image_),
                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT),
                    VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                    VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT),

                vkrndr::with_access(
                    vkrndr::on_stage(vkrndr::image_barrier(blur_image),
                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT),
                    VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                    VK_ACCESS_2_SHADER_SAMPLED_READ_BIT)};

            vkrndr::wait_for(command_buffer, {}, {}, barriers);
        }

        weighted_blend_shader_->draw(bloom_strength_,
            command_buffer,
            resolve_image_,
            blur_image);

        {
            std::array const barriers{
                vkrndr::with_access(
                    vkrndr::on_stage(vkrndr::image_barrier(resolve_image_),
                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT),
                    VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                    VK_ACCESS_2_SHADER_STORAGE_READ_BIT),

                vkrndr::to_layout(
                    vkrndr::with_access(
                        vkrndr::on_stage(vkrndr::image_barrier(target_image),
                            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT),
                        VK_ACCESS_2_NONE,
                        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT),
                    VK_IMAGE_LAYOUT_GENERAL)};

            vkrndr::wait_for(command_buffer, {}, {}, barriers);
        }

        postprocess_shader_->draw(color_conversion_,
            tone_mapping_,
            command_buffer,
            resolve_image_,
            target_image);

        {
            auto const barrier{vkrndr::with_layout(
                vkrndr::with_access(
                    vkrndr::on_stage(vkrndr::image_barrier(target_image),
                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT),
                    VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                    VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT),
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)};
            vkrndr::wait_for(command_buffer, {}, {}, cppext::as_span(barrier));
        }
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
    mouse_.set_window_handle(window()->native_handle());

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

    shadow_map_.reset();

    depth_pass_shader_.reset();

    scene_graph_.reset();

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
    VKRNDR_IF_DEBUG_UTILS(
        object_name(backend_->device(), color_image_, "Offscreen Image"));

    destroy(&backend_->device(), &depth_buffer_);
    depth_buffer_ = create_depth_buffer(*backend_);
    VKRNDR_IF_DEBUG_UTILS(
        object_name(backend_->device(), depth_buffer_, "Depth Buffer"));

    destroy(&backend_->device(), &resolve_image_);
    resolve_image_ = create_resolve_image(*backend_);
    VKRNDR_IF_DEBUG_UTILS(
        object_name(backend_->device(), resolve_image_, "Resolve Image"));

    weighted_oit_shader_->resize(width, height);

    pyramid_blur_->resize(width, height);

    projection_.set_aspect_ratio(cppext::as_fp(width) / cppext::as_fp(height));
}
