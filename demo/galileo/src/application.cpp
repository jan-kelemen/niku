#include <application.hpp>

#include <camera_controller.hpp>

#include <cppext_numeric.hpp>
#include <cppext_overloaded.hpp>
#include <cppext_pragma_warning.hpp>

#include <niku_application.hpp>
#include <niku_imgui_layer.hpp>
#include <niku_perspective_camera.hpp>
#include <niku_sdl_window.hpp> // IWYU pragma: keep

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

#include <Jolt/Jolt.h>

#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/MotionType.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>

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

namespace
{
    [[nodiscard]] std::vector<JPH::BodyID> setup_world(
        JPH::BodyInterface& body_interface)
    {
        using namespace JPH::literals;

        // Next we can create a rigid body to serve as the floor, we make a
        // large box Create the settings for the collision volume (the shape).
        // Note that for simple shapes (like boxes) you can also directly
        // construct a BoxShape.
        JPH::BoxShapeSettings const floor_shape_settings{
            JPH::Vec3{100.0f, 1.0f, 100.0f}};
        floor_shape_settings
            .SetEmbedded(); // A ref counted object on the stack (base class
                            // RefTarget) should be marked as such to prevent it
                            // from being freed when its reference count goes to
                            // 0.

        // Create the shape
        JPH::ShapeSettings::ShapeResult const floor_shape_result{
            floor_shape_settings.Create()};
        JPH::ShapeRefC const floor_shape{floor_shape_result
                .Get()}; // We don't expect an error here, but you can check
                         // floor_shape_result for HasError() / GetError()

        // Create the settings for the body itself. Note that here you can also
        // set other properties like the restitution / friction.
        JPH::BodyCreationSettings const floor_settings{floor_shape,
            JPH::RVec3{0.0_r, -1.0_r, 0.0_r},
            JPH::Quat::sIdentity(),
            JPH::EMotionType::Static,
            galileo::object_layers::non_moving};

        // Create the actual rigid body
        JPH::Body* floor{body_interface.CreateBody(
            floor_settings)}; // Note that if we run out of bodies this can
                              // return nullptr

        // Add it to the world
        body_interface.AddBody(floor->GetID(), JPH::EActivation::DontActivate);

        // Now create a dynamic body to bounce on the floor
        // Note that this uses the shorthand version of creating and adding a
        // body to the world
        JPH::BodyCreationSettings const sphere_settings{
            new JPH::SphereShape{0.5f},
            JPH::RVec3{0.0_r, 2.0_r, 0.0_r},
            JPH::Quat::sIdentity(),
            JPH::EMotionType::Dynamic,
            galileo::object_layers::moving};

        JPH::BodyID const sphere_id =
            body_interface.CreateAndAddBody(sphere_settings,
                JPH::EActivation::Activate);

        // Now you can interact with the dynamic body, in this case we're going
        // to give it a velocity. (note that if we had used CreateBody then we
        // could have set the velocity straight on the body before adding it to
        // the physics system)
        body_interface.SetLinearVelocity(sphere_id,
            JPH::Vec3{0.0f, -5.0f, 0.0f});

        std::vector<JPH::BodyID> rv;
        rv.emplace_back(floor->GetID());
        rv.emplace_back(sphere_id);
        return rv;
    }
} // namespace

galileo::application_t::application_t(bool const debug)
    : niku::application_t{niku::startup_params_t{
          .init_subsystems = {.video = true, .debug = debug},
          .title = "galileo",
          .window_flags = static_cast<SDL_WindowFlags>(
              SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI),
          .centered = true,
          .width = 512,
          .height = 512}}
    , backend_{std::make_unique<vkrndr::backend_t>(*window(),
          vkrndr::render_settings_t{
              .preferred_swapchain_format = VK_FORMAT_R8G8B8A8_SRGB,
              .preferred_present_mode = VK_PRESENT_MODE_FIFO_KHR,
          },
          debug)}
    , imgui_{std::make_unique<niku::imgui_layer_t>(*window(),
          backend_->context(),
          backend_->device(),
          backend_->swap_chain())}
    , camera_controller_{camera_, mouse_}
{
    fixed_update_interval(1.0f / 60.0f);
}

galileo::application_t::~application_t() = default;

bool galileo::application_t::handle_event(SDL_Event const& event)
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

void galileo::application_t::fixed_update(float const delta_time)
{
    auto& body_interface{physics_engine_.body_interface()};
    for (auto const& body_id : bodies_)
    {
        JPH::RVec3 const position{
            body_interface.GetCenterOfMassPosition(body_id)};
        JPH::Vec3 const velocity{body_interface.GetLinearVelocity(body_id)};
        spdlog::info("Body {}: Position = ({}, {}, {}), Velocity =({}, {}, {})",
            body_id.GetIndexAndSequenceNumber(),
            position.GetX(),
            position.GetY(),
            position.GetZ(),
            velocity.GetX(),
            velocity.GetY(),
            velocity.GetZ());
    }

    physics_engine_.fixed_update(delta_time);
}

void galileo::application_t::update(float delta_time)
{
    camera_controller_.update(delta_time);
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

    vkrndr::wait_for_color_attachment_read(target_image.image, command_buffer);

    VkViewport const viewport{.x = 0.0f,
        .y = 0.0f,
        .width = cppext::as_fp(target_image.extent.width),
        .height = cppext::as_fp(target_image.extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f};
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D const scissor{{0, 0}, target_image.extent};
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

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
    bodies_ = setup_world(physics_engine_.body_interface());

    auto const extent{backend_->extent()};
    on_resize(extent.width, extent.height);
}

void galileo::application_t::on_shutdown()
{
    vkDeviceWaitIdle(backend_->device().logical);

    imgui_.reset();

    backend_.reset();
}

void galileo::application_t::on_resize(uint32_t const width,
    uint32_t const height)
{
    camera_.set_aspect_ratio(cppext::as_fp(width) / cppext::as_fp(height));
}
