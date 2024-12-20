#include <application.hpp>

#include <camera_controller.hpp>
#include <character.hpp>
#include <deferred_shader.hpp>
#include <frame_info.hpp>
#include <gbuffer.hpp>
#include <gbuffer_shader.hpp>
#include <materials.hpp>
#include <physics_debug.hpp>
#include <physics_engine.hpp>
#include <postprocess_shader.hpp>
#include <render_graph.hpp>

#include <cppext_numeric.hpp>
#include <cppext_overloaded.hpp>
#include <cppext_pragma_warning.hpp>

#include <ngngfx_perspective_camera.hpp>

#include <ngnphy_jolt_adapter.hpp>

#include <ngnwsi_application.hpp>
#include <ngnwsi_imgui_layer.hpp>
#include <ngnwsi_sdl_window.hpp> // IWYU pragma: keep

#include <vkgltf_loader.hpp>
#include <vkgltf_model.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_commands.hpp>
#include <vkrndr_debug_utils.hpp>
#include <vkrndr_depth_buffer.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_render_settings.hpp>

#include <boost/scope/defer.hpp>

DISABLE_WARNING_PUSH
DISABLE_WARNING_STRINGOP_OVERFLOW
#include <fmt/std.h> // IWYU pragma: keep
DISABLE_WARNING_POP

#include <imgui.h>

#include <glm/vec3.hpp>

#include <Jolt/Jolt.h> // IWYU pragma: keep

#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Real.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/MotionType.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/EActivation.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_scancode.h>
#include <SDL2/SDL_video.h>

#include <spdlog/spdlog.h>

#include <volk.h>

#include <cstddef>
#include <cstdint>
#include <exception>
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
// IWYU pragma: no_include <system_error>

namespace
{
    [[nodiscard]] vkrndr::image_t create_color_image(
        vkrndr::backend_t const& backend)
    {
        return vkrndr::create_image_and_view(backend.device(),
            backend.extent(),
            1,
            VK_SAMPLE_COUNT_1_BIT,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT);
    }

    [[nodiscard]] vkrndr::image_t create_depth_buffer(
        vkrndr::backend_t const& backend)
    {
        return vkrndr::create_depth_buffer(backend.device(),
            backend.extent(),
            false,
            VK_SAMPLE_COUNT_1_BIT);
    }

    [[nodiscard]] std::vector<std::pair<size_t, JPH::BodyID>> setup_world(
        vkrndr::backend_t& backend,
        galileo::materials_t& materials,
        galileo::render_graph_t& graph,
        JPH::BodyInterface& body_interface)
    {
        using namespace JPH::literals;

        vkgltf::loader_t loader{backend};
        auto model{loader.load(std::filesystem::absolute("world.glb"))};
        if (!model)
        {
            std::terminate();
        }
        [[maybe_unused]] boost::scope::defer_guard const guard{
            [d = &backend.device(), m = &model.value()]() { destroy(d, m); }};

        spdlog::info("nodes: {} meshes: {}",
            model->nodes.size(),
            model->meshes.size());
        graph.consume(*model);

        make_node_matrices_absolute(*model);

        std::vector<std::pair<size_t, JPH::BodyID>> rv;
        for (vkgltf::scene_graph_t const& scene : model->scenes)
        {
            for (size_t const& root_index : scene.root_indices)
            {
                auto const& root{model->nodes[root_index]};
                if (!root.aabb)
                {
                    std::terminate();
                }

                // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
                glm::vec3 const half_extents{
                    (root.aabb->max - root.aabb->min) / 2.0f};

                if (root.name == "Cube")
                {
                    JPH::BoxShapeSettings const floor_shape_settings{
                        ngnphy::to_jolt(half_extents)};
                    floor_shape_settings.SetEmbedded();

                    JPH::ShapeSettings::ShapeResult const floor_shape_result{
                        floor_shape_settings.Create()};
                    JPH::ShapeRefC const floor_shape{floor_shape_result.Get()};

                    JPH::BodyCreationSettings const floor_settings{floor_shape,
                        ngnphy::to_jolt(glm::vec3{root.matrix[3]}),
                        JPH::Quat::sIdentity(),
                        JPH::EMotionType::Static,
                        galileo::object_layers::non_moving};

                    JPH::Body* floor{body_interface.CreateBody(floor_settings)};

                    body_interface.AddBody(floor->GetID(),
                        JPH::EActivation::DontActivate);

                    rv.emplace_back(root_index, floor->GetID());
                }
                else if (root.name.starts_with("Icosphere"))
                {
                    JPH::BodyCreationSettings const sphere_settings{
                        new JPH::SphereShape{half_extents.x},
                        ngnphy::to_jolt(glm::vec3{root.matrix[3]}),
                        JPH::Quat::sIdentity(),
                        JPH::EMotionType::Dynamic,
                        galileo::object_layers::moving};

                    JPH::BodyID const sphere_id =
                        body_interface.CreateAndAddBody(sphere_settings,
                            JPH::EActivation::Activate);

                    rv.emplace_back(root_index, sphere_id);
                }
            }
        }

        materials.consume(*model);

        return rv;
    }
} // namespace

galileo::application_t::application_t(bool const debug)
    : ngnwsi::application_t{ngnwsi::startup_params_t{
          .init_subsystems = {.video = true, .debug = debug},
          .title = "galileo",
          .window_flags = static_cast<SDL_WindowFlags>(
              SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI),
          .centered = true,
          .width = 512,
          .height = 512}}
    , camera_controller_{camera_, mouse_}
    , follow_camera_controller_{camera_}
    , backend_{std::make_unique<vkrndr::backend_t>(*window(),
          vkrndr::render_settings_t{
              .preferred_swapchain_format = VK_FORMAT_B8G8R8A8_UNORM,
              .swapchain_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                  VK_IMAGE_USAGE_STORAGE_BIT,
              .preferred_present_mode = VK_PRESENT_MODE_FIFO_KHR},
          debug)}
    , imgui_{std::make_unique<ngnwsi::imgui_layer_t>(*window(),
          backend_->context(),
          backend_->device(),
          backend_->swap_chain())}
    , depth_buffer_{create_depth_buffer(*backend_)}
    , gbuffer_{std::make_unique<gbuffer_t>(*backend_)}
    , color_image_{create_color_image(*backend_)}
    , frame_info_{std::make_unique<frame_info_t>(*backend_)}
    , materials_{std::make_unique<materials_t>(*backend_)}
    , render_graph_{std::make_unique<render_graph_t>(*backend_)}
    , deferred_shader_{std::make_unique<deferred_shader_t>(*backend_,
          frame_info_->descriptor_set_layout())}
    , postprocess_shader_{std::make_unique<postprocess_shader_t>(*backend_)}
    , physics_debug_{std::make_unique<physics_debug_t>(*backend_,
          frame_info_->descriptor_set_layout(),
          depth_buffer_.format)}
{
    camera_.set_position({-25.0f, 5.0f, -25.0f});

    physics_debug_->set_camera(camera_);
}

galileo::application_t::~application_t() = default;

bool galileo::application_t::handle_event(SDL_Event const& event,
    float const delta_time)
{
    // camera_controller_.handle_event(event);
    character_->handle_event(event, delta_time);

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

void galileo::application_t::update(float const delta_time)
{
    ImGui::Begin("Lights");
    ImGui::SliderInt("Count", &light_count_, 0, 1000);
    ImGui::End();

    // camera_controller_.update(delta_time);

    character_->update(delta_time);

    follow_camera_controller_.update(*character_);

    physics_engine_.update(delta_time);

    auto& body_interface{physics_engine_.body_interface()};
    for (auto const& [index, body_id] : bodies_)
    {
        render_graph_->update(index,
            ngnphy::to_glm(body_interface.GetWorldTransform(body_id)));
    }

    physics_engine_.physics_system().DrawBodies({}, physics_debug_.get());
    character_->debug(physics_debug_.get());

    frame_info_->update(camera_, static_cast<uint32_t>(light_count_));
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

    VkViewport const viewport{.x = 0.0f,
        .y = 0.0f,
        .width = cppext::as_fp(target_image.extent.width),
        .height = cppext::as_fp(target_image.extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f};
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D const scissor{{0, 0}, target_image.extent};
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    if (VkPipelineLayout const layout{gbuffer_shader_->pipeline_layout()})
    {
        gbuffer_->transition(command_buffer,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

        frame_info_->bind_on(command_buffer,
            layout,
            VK_PIPELINE_BIND_POINT_GRAPHICS);

        materials_->bind_on(command_buffer,
            layout,
            VK_PIPELINE_BIND_POINT_GRAPHICS);

        render_graph_->bind_on(command_buffer,
            layout,
            VK_PIPELINE_BIND_POINT_GRAPHICS);

        gbuffer_shader_->draw(*render_graph_,
            command_buffer,
            *gbuffer_,
            depth_buffer_);
    }

    vkrndr::wait_for_color_attachment_write(color_image_.image, command_buffer);

    if (VkPipelineLayout const layout{deferred_shader_->pipeline_layout()})
    {
        gbuffer_->transition(command_buffer,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

        frame_info_->bind_on(command_buffer,
            layout,
            VK_PIPELINE_BIND_POINT_GRAPHICS);

        deferred_shader_->draw(command_buffer, *gbuffer_, color_image_);

        vkrndr::transition_image(color_image_.image,
            command_buffer,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
            1);
    }

    if (VkPipelineLayout const layout{physics_debug_->pipeline_layout()})
    {
        frame_info_->bind_on(command_buffer,
            layout,
            VK_PIPELINE_BIND_POINT_GRAPHICS);
        physics_debug_->draw(command_buffer, color_image_, depth_buffer_);

        vkrndr::transition_image(color_image_.image,
            command_buffer,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            1);
    }

    vkrndr::transition_image(target_image.image,
        command_buffer,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_NONE,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        1);

    postprocess_shader_->draw(command_buffer, color_image_, target_image);

    vkrndr::transition_image(target_image.image,
        command_buffer,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
        1);

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
    bodies_ = setup_world(*backend_,
        *materials_,
        *render_graph_,
        physics_engine_.body_interface());

    character_ = std::make_unique<character_t>(physics_engine_, mouse_);
    character_->set_position({5.0f, 5.0f, 5.0f});

    gbuffer_shader_ = std::make_unique<gbuffer_shader_t>(*backend_,
        frame_info_->descriptor_set_layout(),
        materials_->descriptor_set_layout(),
        render_graph_->descriptor_set_layout(),
        depth_buffer_.format);

    auto const extent{backend_->extent()};
    on_resize(extent.width, extent.height);
}

void galileo::application_t::on_shutdown()
{
    vkDeviceWaitIdle(backend_->device().logical);

    physics_debug_.reset();

    postprocess_shader_.reset();

    deferred_shader_.reset();

    gbuffer_shader_.reset();

    render_graph_.reset();

    materials_.reset();

    frame_info_.reset();

    destroy(&backend_->device(), &color_image_);

    gbuffer_.reset();

    destroy(&backend_->device(), &depth_buffer_);

    imgui_.reset();

    backend_.reset();
}

void galileo::application_t::on_resize(uint32_t const width,
    uint32_t const height)
{
    camera_.set_aspect_ratio(cppext::as_fp(width) / cppext::as_fp(height));

    destroy(&backend_->device(), &depth_buffer_);
    depth_buffer_ = create_depth_buffer(*backend_);

    gbuffer_->resize(width, height);

    destroy(&backend_->device(), &color_image_);
    color_image_ = create_color_image(*backend_);
    object_name(backend_->device(), color_image_, "Color image");

    postprocess_shader_->resize(width, height);
}
