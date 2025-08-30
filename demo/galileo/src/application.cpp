#include <application.hpp>

#include <batch_renderer.hpp>
#include <camera_controller.hpp>
#include <character.hpp>
#include <character_contact_listener.hpp>
#include <deferred_shader.hpp>
#include <follow_camera_controller.hpp>
#include <frame_info.hpp>
#include <gbuffer.hpp>
#include <gbuffer_shader.hpp>
#include <materials.hpp>
#include <navmesh.hpp>
#include <navmesh_debug.hpp>
#include <physics_debug.hpp>
#include <physics_engine.hpp>
#include <postprocess_shader.hpp>
#include <scene_graph.hpp>
#include <scripting.hpp>
#include <sphere.hpp>
#include <world.hpp>
#include <world_contact_listener.hpp>

#include <cppext_container.hpp>
#include <cppext_numeric.hpp>
#include <cppext_pragma_warning.hpp>
#include <cppext_thread_pool.hpp>

#include <ngnast_gltf_loader.hpp>
#include <ngnast_scene_model.hpp>

#include <ngngfx_aircraft_camera.hpp>
#include <ngngfx_perspective_projection.hpp>

#include <ngnphy_coordinate_system.hpp>
#include <ngnphy_jolt_adapter.hpp>
#include <ngnphy_jolt_geometry.hpp>

#include <ngnscr_script_compiler.hpp>
#include <ngnscr_scripting_engine.hpp>

#include <ngnwsi_application.hpp>
#include <ngnwsi_imgui_layer.hpp>
#include <ngnwsi_mouse.hpp>
#include <ngnwsi_render_window.hpp>
#include <ngnwsi_sdl_window.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_commands.hpp>
#include <vkrndr_cpu_pacing.hpp>
#include <vkrndr_debug_utils.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_error_code.hpp>
#include <vkrndr_execution_port.hpp>
#include <vkrndr_features.hpp>
#include <vkrndr_formats.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_instance.hpp>
#include <vkrndr_library_handle.hpp>
#include <vkrndr_render_pass.hpp>
#include <vkrndr_swapchain.hpp>
#include <vkrndr_synchronization.hpp>
#include <vkrndr_utility.hpp>

#include <angelscript.h>

#include <fmt/format.h>
#include <fmt/ranges.h>
DISABLE_WARNING_PUSH
DISABLE_WARNING_STRINGOP_OVERFLOW
#include <fmt/std.h> // IWYU pragma: keep
DISABLE_WARNING_POP

#include <imgui.h>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

#include <Jolt/Core/Core.h>
#include <Jolt/Geometry/IndexedTriangle.h>
#include <Jolt/Geometry/Triangle.h>
#include <Jolt/Math/Float3.h>
#include <Jolt/Math/Real.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyManager.h>
#include <Jolt/Physics/Body/MotionType.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/EActivation.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <recastnavigation/DetourNavMesh.h>
#include <recastnavigation/DetourPathCorridor.h>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_video.h>

#include <spdlog/spdlog.h>

#include <vma_impl.hpp>

#include <volk.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <expected>
#include <filesystem>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

// IWYU pragma: no_include <boost/smart_ptr/intrusive_ref_counter.hpp>
// IWYU pragma: no_include <fmt/base.h>
// IWYU pragma: no_include <glm/detail/qualifier.hpp>
// IWYU pragma: no_include <recastnavigation/DetourNavMeshQuery.h>
// IWYU pragma: no_include <map>
// IWYU pragma: no_include <optional>
// IWYU pragma: no_include <ratio>
// IWYU pragma: no_include <string_view>

namespace
{
    [[nodiscard]] vkrndr::image_t create_color_image(
        vkrndr::backend_t const& backend,
        VkExtent2D const extent)
    {
        return vkrndr::create_image_and_view(backend.device(),
            vkrndr::image_2d_create_info_t{
                .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                .extent = extent,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                    VK_IMAGE_USAGE_SAMPLED_BIT,
                .allocation_flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                .required_memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
            VK_IMAGE_ASPECT_COLOR_BIT);
    }

    [[nodiscard]] vkrndr::image_t create_depth_buffer(
        vkrndr::backend_t const& backend,
        VkExtent2D const extent)
    {
        auto const& formats{
            vkrndr::find_supported_depth_stencil_formats(backend.device(),
                true,
                false)};
        vkrndr::image_2d_create_info_t create_info{.extent = extent,
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

    [[nodiscard]] JPH::MeshShapeSettings to_jolt_mesh_shape(
        ngnast::primitive_t const& primitive)
    {
        auto const to_jolt_pos = [](ngnast::vertex_t const& vtx) -> JPH::Float3
        { return {vtx.position[0], vtx.position[1], vtx.position[2]}; };

        if (!primitive.indices.empty())
        {
            auto const to_jolt_tri =
                [](uint32_t const p1,
                    uint32_t const p2,
                    uint32_t const p3) -> JPH::IndexedTriangle
            { return {p1, p2, p3}; };

            return {ngnphy::to_vertices(primitive.vertices, to_jolt_pos),
                ngnphy::to_indexed_triangles(primitive.indices, to_jolt_tri)};
        }

        auto const to_jolt_tri =
            [&to_jolt_pos](ngnast::vertex_t const& v1,
                ngnast::vertex_t const& v2,
                ngnast::vertex_t const& v3) -> JPH::Triangle
        { return {to_jolt_pos(v1), to_jolt_pos(v2), to_jolt_pos(v3)}; };

        return {
            ngnphy::to_unindexed_triangles(primitive.vertices, to_jolt_tri)};
    }
} // namespace

galileo::application_t::application_t(bool const debug)
    : ngnwsi::application_t{ngnwsi::startup_params_t{
          .init_subsystems = {.video = true, .debug = debug}}}
    , free_camera_controller_{camera_, mouse_}
    , follow_camera_controller_{camera_}
    , random_engine_{std::random_device{}()}
    , physics_engine_{thread_pool_}
    , polymesh_params_{.walkable_slope_angle = character_t::max_slope_angle,
          .walkable_radius = 1.0f}
    , world_{physics_engine_}
    , world_listener_{std::make_unique<world_contact_listener_t>(
          scripting_engine_,
          registry_)}
    , render_window_{std::make_unique<ngnwsi::render_window_t>("galileo",
          SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY,
          512,
          512)}
{
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
                        .application_name = "galileo",
                    });
                })
            .transform(
                [this](vkrndr::instance_ptr_t&& instance)
                {
                    spdlog::info(
                        "Created with instance handle {}.\n\tEnabled extensions: {}\n\tEnabled layers: {}",
                        std::bit_cast<intptr_t>(instance->handle),
                        fmt::join(instance->extensions, ", "),
                        fmt::join(instance->layers, ", "));

                    rendering_context_.instance = std::move(instance);
                })
            .transform_error(
                [](std::error_code ec)
                {
                    spdlog::error("Instance creation failed with: {}",
                        ec.message());
                    return ec;
                })
            .and_then(
                [this]() -> std::expected<vkrndr::device_ptr_t, std::error_code>
                {
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
                        return std::unexpected{vkrndr::make_error_code(
                            VK_ERROR_INITIALIZATION_FAILED)};
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
                        return std::unexpected{vkrndr::make_error_code(
                            VK_ERROR_INITIALIZATION_FAILED)};
                    }

                    return create_device(*rendering_context_.instance,
                        device_extensions,
                        *physical_device,
                        cppext::as_span(*queue_with_present));
                })
            .transform(
                [this](vkrndr::device_ptr_t&& device)
                {
                    spdlog::info(
                        "Created with device handle {}.\n\tEnabled extensions: {}",
                        std::bit_cast<intptr_t>(device->logical_device),
                        fmt::join(device->extensions, ", "));

                    rendering_context_.device = std::move(device);
                })
            .transform_error(
                [](std::error_code ec)
                {
                    spdlog::error("Device creation failed with: {}",
                        ec.message());
                    return ec;
                })
            .transform(
                [this]()
                {
                    backend_ =
                        std::make_unique<vkrndr::backend_t>(rendering_context_,
                            2);

                    vkrndr::execution_port_t& present_queue{
                        *std::ranges::find_if(
                            rendering_context_.device->execution_ports,
                            &vkrndr::execution_port_t::has_present)};

                    vkrndr::swapchain_t* const swapchain{
                        render_window_->create_swapchain(
                            *rendering_context_.device,
                            {
                                .preferred_format = VK_FORMAT_B8G8R8A8_UNORM,
                                .image_flags =
                                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                    VK_IMAGE_USAGE_STORAGE_BIT,
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

                    depth_buffer_ =
                        create_depth_buffer(*backend_, swapchain->extent());
                    gbuffer_ = std::make_unique<gbuffer_t>(*backend_);
                    color_image_ =
                        create_color_image(*backend_, swapchain->extent());
                    frame_info_ = std::make_unique<frame_info_t>(*backend_);
                    materials_ = std::make_unique<materials_t>(*backend_);
                    scene_graph_ = std::make_unique<scene_graph_t>(*backend_);
                    deferred_shader_ =
                        std::make_unique<deferred_shader_t>(*backend_,
                            frame_info_->descriptor_set_layout());
                    postprocess_shader_ =
                        std::make_unique<postprocess_shader_t>(*backend_);
                    batch_renderer_ =
                        std::make_unique<batch_renderer_t>(*backend_,
                            frame_info_->descriptor_set_layout(),
                            depth_buffer_.format);
                    physics_debug_ =
                        std::make_unique<physics_debug_t>(*batch_renderer_);
                    navmesh_debug_ =
                        std::make_unique<navmesh_debug_t>(*batch_renderer_);
                })};
    if (!create_result)
    {
        throw std::system_error{create_result.error()};
    }

    thread_pool_.set_thread_exit_function([]() { asThreadCleanup(); });

    camera_.set_position({-25.0f, 5.0f, -25.0f});

    physics_engine_.physics_system().SetContactListener(world_listener_.get());

    physics_debug_->set_camera(camera_);
}

galileo::application_t::~application_t() = default;

bool galileo::application_t::should_run()
{
    return static_cast<bool>(render_window_);
}

bool galileo::application_t::handle_event(SDL_Event const& event)
{
    if (event.type == SDL_EVENT_WINDOW_RESIZED)
    {
        on_resize(cppext::narrow<uint32_t>(event.window.data1),
            cppext::narrow<uint32_t>(event.window.data2));
        return true;
    }

    [[maybe_unused]] auto imgui_handled{imgui_->handle_event(event)};

    if (event.type == SDL_EVENT_QUIT ||
        event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
    {
        if (render_window_)
        {
            vkDeviceWaitIdle(backend_->device());

            render_window_->destroy_swapchain();
            render_window_->destroy_surface(*rendering_context_.instance);
            render_window_.reset();
        }

        return true;
    }

    if (free_camera_active_ && event.type != SDL_EVENT_MOUSE_BUTTON_DOWN)
    {
        free_camera_controller_.handle_event(event);
    }
    else if (character_action_t const action{character_->handle_event(event)};
        action != character_action_t::none)
    {
        assert(action == character_action_t::select_body);

        if (auto const body{world_.cast_ray(character_->center_of_mass(),
                character_->rotation() *
                    ngnphy::coordinate_system_t::front *
                    2.0f)})
        {
            auto& body_interface{physics_engine_.body_interface()};

            auto const entity{
                static_cast<entt::entity>(body_interface.GetUserData(*body))};

            if (!registry_.try_get<component::sphere_path_t>(entity))
            {
                spdlog::info("selected body {} ",
                    body->GetIndexAndSequenceNumber());

                navigation_mesh_query_ptr_t query{
                    world_.get_navigation_query()};

                glm::vec3 const spawner_position{
                    ngnphy::to_glm(body_interface.GetPosition(
                        registry_.get<component::physics_t>(spawner_).id))};

                glm::vec3 const sphere_position{
                    ngnphy::to_glm(body_interface.GetPosition(*body))};

                std::optional<path_t> navmesh_path{find_path(*query,
                    sphere_position,
                    spawner_position,
                    (world_aabb_.max - world_aabb_.min) / 2.0f)};

                if (navmesh_path)
                {
                    registry_.emplace<component::sphere_path_t>(entity,
                        std::move(query),
                        *std::move(navmesh_path));
                }
            }
        }
    }

    if (event.type == SDL_EVENT_KEY_DOWN)
    {
        auto const& keyboard{event.key};
        if (keyboard.scancode == SDL_SCANCODE_F3)
        {
            mouse_.set_capture(!mouse_.captured());
        }
        else if (keyboard.scancode == SDL_SCANCODE_F4)
        {
            imgui_->set_enabled(!imgui_->enabled());
        }
        else if (keyboard.scancode == SDL_SCANCODE_C)
        {
            free_camera_active_ ^= true; // NOLINT
        }
    }

    return true;
}

void galileo::application_t::update(float const delta_time)
{
    character_->update(delta_time);

    if (update_navmesh_)
    {
        try
        {
            auto const now{std::chrono::system_clock::now()};
            poly_mesh_ = generate_poly_mesh(polymesh_params_,
                world_primitive_,
                world_aabb_);

            world_.update_navigation_mesh(
                generate_navigation_mesh(polymesh_params_,
                    poly_mesh_,
                    world_aabb_));

            auto const diff{std::chrono::system_clock::now() - now};
            spdlog::info("Navigation mesh generation took {}",
                std::chrono::duration_cast<std::chrono::milliseconds>(diff));
        }
        catch (std::exception const& ex)
        {
            spdlog::error(ex.what());
        }

        update_navmesh_ = false;

        registry_.clear<component::sphere_path_t>();
    }

    if (free_camera_active_)
    {
        free_camera_controller_.update(delta_time);
    }
    else
    {
        follow_camera_controller_.update(*character_);
    }
    projection_.update(camera_.view_matrix());

    move_spheres(registry_, physics_engine_);

    physics_engine_.update(delta_time);
}

void galileo::application_t::draw()
{
    std::optional<vkrndr::image_t> const target_image{
        render_window_->acquire_next_image()};
    if (!target_image)
    {
        return;
    }

    backend_->begin_frame();

    imgui_->begin_frame();
    frame_info_->begin_frame();
    batch_renderer_->begin_frame();
    scene_graph_->begin_frame();

    debug_draw();

    auto& body_interface{physics_engine_.body_interface()};
    for (auto const entity :
        registry_.view<component::mesh_t, component::physics_t>())
    {
        scene_graph_->update(registry_.get<component::mesh_t>(entity).index,
            ngnphy::to_glm(body_interface.GetWorldTransform(
                registry_.get<component::physics_t>(entity).id)));
    }

    for (auto const entity :
        registry_.view<component::mesh_t, component::character_t>())
    {
        scene_graph_->update(registry_.get<component::mesh_t>(entity).index,
            character_->world_transform());
    }

    frame_info_->update(camera_,
        projection_,
        static_cast<uint32_t>(light_count_));

    if (draw_physics_)
    {
        JPH::BodyManager::DrawSettings const ds{.mDrawVelocity = true};
        physics_engine_.physics_system().DrawBodies(ds, physics_debug_.get());
        character_->debug(physics_debug_.get());
    }

    if (draw_main_polymesh_)
    {
        navmesh_debug_->draw_poly_mesh(*poly_mesh_.mesh);
    }

    if (draw_detail_polymesh_)
    {
        navmesh_debug_->draw_detail_poly_mesh(*poly_mesh_.detail_mesh);
    }

    if (draw_navigation_mesh_)
    {
        navmesh_debug_->draw_navigation_mesh(world_.get_navigation_mesh());
    }

    if (draw_navigation_queries_)
    {
        for (auto const entity : registry_.view<component::sphere_path_t>())
        {
            auto const& path{registry_.get<component::sphere_path_t>(entity)};
            navmesh_debug_->draw_nodes(*path.navmesh_query);
            std::ranges::for_each(
                std::span{path.navmesh_path.corridor->getPath(),
                    cppext::narrow<size_t>(
                        path.navmesh_path.corridor->getPathCount())},
                [this, &nm = world_.get_navigation_mesh()](dtPolyRef const r)
                { navmesh_debug_->draw_poly(nm, r); });
        }
    }

    VkCommandBuffer command_buffer{backend_->request_command_buffer()};

    VkViewport const viewport{.x = 0.0f,
        .y = 0.0f,
        .width = cppext::as_fp(target_image->extent.width),
        .height = cppext::as_fp(target_image->extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f};
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D const scissor{{0, 0}, vkrndr::to_2d_extent(target_image->extent)};
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    {
        gbuffer_->transition(command_buffer,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

        vkrndr::render_pass_t geometry_pass;
        geometry_pass.with_color_attachment(VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            gbuffer_->position_image().view,
            VkClearValue{.color = {{0.0f, 0.0f, 0.0f, 1.0f}}});
        geometry_pass.with_color_attachment(VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            gbuffer_->normal_image().view,
            VkClearValue{.color = {{0.0f, 0.0f, 0.0f, 1.0f}}});
        geometry_pass.with_color_attachment(VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            gbuffer_->albedo_image().view,
            VkClearValue{.color = {{0.0f, 0.0f, 0.0f, 1.0f}}});
        geometry_pass.with_depth_attachment(VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            depth_buffer_.view,
            VkClearValue{.depthStencil = {1.0f, 0}});

        [[maybe_unused]] auto const guard{
            geometry_pass.begin(command_buffer, {{0, 0}, gbuffer_->extent()})};

        if (VkPipelineLayout const layout{gbuffer_shader_->pipeline_layout()})
        {
            frame_info_->bind_on(command_buffer,
                layout,
                VK_PIPELINE_BIND_POINT_GRAPHICS);

            materials_->bind_on(command_buffer,
                layout,
                VK_PIPELINE_BIND_POINT_GRAPHICS);

            scene_graph_->bind_on(command_buffer,
                layout,
                VK_PIPELINE_BIND_POINT_GRAPHICS);

            gbuffer_shader_->draw(*scene_graph_, command_buffer);
        }
    }

    {
        {
            gbuffer_->transition(command_buffer,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);

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

        vkrndr::render_pass_t lighting_pass;
        lighting_pass.with_color_attachment(VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            color_image_.view);

        [[maybe_unused]] auto const guard{lighting_pass.begin(command_buffer,
            {{0, 0}, vkrndr::to_2d_extent(color_image_.extent)})};

        if (VkPipelineLayout const layout{deferred_shader_->pipeline_layout()})
        {
            frame_info_->bind_on(command_buffer,
                layout,
                VK_PIPELINE_BIND_POINT_GRAPHICS);

            deferred_shader_->draw(command_buffer, *gbuffer_);
        }
    }

    {
        auto const barrier{vkrndr::with_access(
            vkrndr::on_stage(vkrndr::image_barrier(color_image_),
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT),
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT)};
        vkrndr::wait_for(command_buffer, {}, {}, cppext::as_span(barrier));

        vkrndr::render_pass_t debug_pass;
        debug_pass.with_color_attachment(VK_ATTACHMENT_LOAD_OP_LOAD,
            VK_ATTACHMENT_STORE_OP_STORE,
            color_image_.view);
        debug_pass.with_depth_attachment(VK_ATTACHMENT_LOAD_OP_LOAD,
            VK_ATTACHMENT_STORE_OP_NONE,
            depth_buffer_.view);

        [[maybe_unused]] auto const guard{debug_pass.begin(command_buffer,
            {{0, 0}, vkrndr::to_2d_extent(target_image->extent)})};
        if (VkPipelineLayout const layout{batch_renderer_->pipeline_layout()})
        {
            frame_info_->bind_on(command_buffer,
                layout,
                VK_PIPELINE_BIND_POINT_GRAPHICS);

            batch_renderer_->draw(command_buffer);
        }
    }

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
                    vkrndr::on_stage(vkrndr::image_barrier(*target_image),
                        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT),
                    VK_ACCESS_2_NONE,
                    VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT),
                VK_IMAGE_LAYOUT_GENERAL)};
        vkrndr::wait_for(command_buffer, {}, {}, barriers);

        postprocess_shader_->draw(command_buffer, color_image_, *target_image);
    }

    {
        auto const barrier{vkrndr::with_layout(
            vkrndr::with_access(
                vkrndr::on_stage(vkrndr::image_barrier(*target_image),
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT),
                VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT),
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)};
        vkrndr::wait_for(command_buffer, {}, {}, cppext::as_span(barrier));

        imgui_->render(command_buffer, *target_image);
    }

    vkrndr::transition_to_present_layout(target_image->image, command_buffer);

    render_window_->present(backend_->present_buffers());

    backend_->end_frame();
}

void galileo::application_t::debug_draw()
{
    ImGui::ShowMetricsWindow();

    ImGui::Begin("Lights");
    ImGui::SliderInt("Count", &light_count_, 0, 2000);
    ImGui::End();

    ImGui::Begin("Debug");
    ImGui::Checkbox("Draw Physics", &draw_physics_);
    ImGui::Checkbox("Draw Mesh", &draw_main_polymesh_);
    ImGui::Checkbox("Draw Detail Mesh", &draw_detail_polymesh_);
    ImGui::Checkbox("Draw Navigation Mesh", &draw_navigation_mesh_);
    ImGui::Checkbox("Draw Navigation Queries", &draw_navigation_queries_);
    ImGui::End();

    {
        ImGui::Begin("Navmesh");
        update_navmesh_ |= ImGui::SliderFloat("Cell Size",
            &polymesh_params_.cell_size,
            0.1f,
            1.0f,
            "%.2f");
        update_navmesh_ |= ImGui::SliderFloat("Cell Height",
            &polymesh_params_.cell_height,
            0.1f,
            1.0f,
            "%.2f");

        update_navmesh_ |= ImGui::SliderFloat("Walkable Slope Angle",
            &polymesh_params_.walkable_slope_angle,
            0.02f,
            glm::half_pi<float>(),
            "%.3f");

        update_navmesh_ |= ImGui::SliderFloat("Walkable Height",
            &polymesh_params_.walkable_height,
            0.1f,
            5.0f,
            "%.1f");

        update_navmesh_ |= ImGui::SliderFloat("Walkable Climb",
            &polymesh_params_.walkable_climb,
            0.0f,
            5.0f,
            "%.1f");

        update_navmesh_ |= ImGui::SliderFloat("Walkable Radius",
            &polymesh_params_.walkable_radius,
            0.0f,
            5.0f,
            "%.1f");

        update_navmesh_ |= ImGui::SliderFloat("Max Edge Length",
            &polymesh_params_.max_edge_length,
            0.0f,
            50.0f,
            "%.0f");

        update_navmesh_ |= ImGui::SliderFloat("Max Simplification Error",
            &polymesh_params_.max_simplification_error,
            0.1f,
            3.0f,
            "%.1f");

        update_navmesh_ |= ImGui::SliderFloat("Min Region Size",
            &polymesh_params_.min_region_size,
            0.0f,
            150.0f,
            "%.0f");

        update_navmesh_ |= ImGui::SliderFloat("Merge Region Size",
            &polymesh_params_.merge_region_size,
            0.0f,
            150.0f,
            "%.0f");

        update_navmesh_ |= ImGui::SliderInt("Max Verts Per Poly",
            &polymesh_params_.max_verts_per_poly,
            3,
            12);

        update_navmesh_ |= ImGui::SliderFloat("Detail Sample Distance",
            &polymesh_params_.detail_sample_distance,
            1.0f,
            16.0f,
            "%.0f");

        update_navmesh_ |= ImGui::SliderFloat("Detail Sample Max Error",
            &polymesh_params_.detail_sample_max_error,
            0.0f,
            16.0f,
            "%.0f");

        ImGui::End();
    }

    if (free_camera_active_)
    {
        free_camera_controller_.draw_imgui();
    }
}

void galileo::application_t::end_frame() { imgui_->end_frame(); }

void galileo::application_t::on_startup()
{
    mouse_.set_window_handle(render_window_->platform_window().native_handle());

    {
        [[maybe_unused]] auto registered{
            scripting_engine_.engine().RegisterGlobalFunction(
                "void spawn_sphere()",
                asMETHOD(application_t, spawn_sphere),
                asCALL_THISCALL_ASGLOBAL,
                this)};
        assert(registered);

        registered = scripting_engine_.engine().RegisterGlobalFunction(
            "bool is_spawner(uint32)",
            asMETHOD(application_t, is_spawner),
            asCALL_THISCALL_ASGLOBAL,
            this);
        assert(registered);

        registered = scripting_engine_.engine().RegisterGlobalFunction(
            "void stop_pathfinding(uint32)",
            asMETHOD(application_t, stop_pathfinding),
            asCALL_THISCALL_ASGLOBAL,
            this);
        assert(registered);

        [[maybe_unused]] auto const spawner_registered{
            register_spawner_type(scripting_engine_)};
        assert(spawner_registered);

        ngnscr::script_compiler_t compiler{scripting_engine_};
        [[maybe_unused]] bool script_compiled{compiler.new_module("MyModule")};
        script_compiled &= compiler.add_section("spawner.as");
        script_compiled &= compiler.add_section("sphere.as");
        script_compiled &= compiler.build();
        assert(script_compiled);
    }

    character_ = std::make_unique<character_t>(physics_engine_, mouse_);

    setup_world();

    frame_info_->disperse_lights(world_aabb_);

    {
        auto& spawner_data{registry_.emplace<spawner_data_t>(spawner_)};

        asIScriptModule const* const mod{
            scripting_engine_.engine().GetModule("MyModule")};
        assert(mod);

        if (auto scripts{component::create_spawner_scripts(spawner_data,
                scripting_engine_,
                *mod)})
        {
            registry_.emplace<component::scripts_t>(spawner_,
                std::move(*scripts));
        }
    }

    character_listener_ =
        std::make_unique<character_contact_listener_t>(physics_engine_,
            scripting_engine_,
            registry_);
    character_->set_contact_listener(character_listener_.get());

    gbuffer_shader_ = std::make_unique<gbuffer_shader_t>(*backend_,
        frame_info_->descriptor_set_layout(),
        materials_->descriptor_set_layout(),
        scene_graph_->descriptor_set_layout(),
        depth_buffer_.format);

    auto const extent{render_window_->swapchain().extent()};
    on_resize(extent.width, extent.height);
}

void galileo::application_t::on_shutdown()
{
    vkDeviceWaitIdle(backend_->device());

    registry_.clear();

    physics_debug_.reset();

    batch_renderer_.reset();

    postprocess_shader_.reset();

    deferred_shader_.reset();

    gbuffer_shader_.reset();

    scene_graph_.reset();

    materials_.reset();

    frame_info_.reset();

    destroy(&backend_->device(), &color_image_);

    gbuffer_.reset();

    destroy(&backend_->device(), &depth_buffer_);

    imgui_.reset();

    if (render_window_)
    {
        render_window_->destroy_swapchain();
        render_window_->destroy_surface(*rendering_context_.instance);
        render_window_.reset();
    }

    backend_.reset();

    rendering_context_.device.reset();
    rendering_context_.instance.reset();
    rendering_context_.library_handle.reset();
}

void galileo::application_t::on_resize(uint32_t const width,
    uint32_t const height)
{
    if (!render_window_->resize(width, height))
    {
        return;
    }

    vkrndr::frame_in_flight_t& in_flight{render_window_->frame_in_flight()};
    std::function<void(std::function<void()>)> deletion_queue_insert{
        [&in_flight](std::function<void()> cb)
        { in_flight.cleanup.push_back(std::move(cb)); }};

    projection_.set_aspect_ratio(cppext::as_fp(width) / cppext::as_fp(height));

    deletion_queue_insert(
        [device = &backend_->device(), image = std::move(depth_buffer_)]()
        { destroy(device, &image); });
    depth_buffer_ = create_depth_buffer(*backend_, {width, height});
    VKRNDR_IF_DEBUG_UTILS(
        object_name(backend_->device(), depth_buffer_, "Depth buffer"));

    deletion_queue_insert([gbuffer = gbuffer_.release()]() { delete gbuffer; });
    gbuffer_ = std::make_unique<gbuffer_t>(*backend_);
    gbuffer_->resize(width, height);

    deletion_queue_insert(
        [device = &backend_->device(), image = std::move(color_image_)]()
        { destroy(device, &image); });
    color_image_ = create_color_image(*backend_, {width, height});
    VKRNDR_IF_DEBUG_UTILS(
        object_name(backend_->device(), color_image_, "Color image"));

    postprocess_shader_->resize(width, height, deletion_queue_insert);
}

void galileo::application_t::setup_world()
{
    using namespace JPH::literals;

    ngnast::gltf::loader_t loader;
    auto model{loader.load(std::filesystem::absolute("world.glb"))};
    if (!model)
    {
        std::terminate();
    }

    spdlog::info("nodes: {} meshes: {}",
        model->nodes.size(),
        model->meshes.size());
    materials_->consume(*model);
    scene_graph_->consume(*model);

    make_node_matrices_absolute(*model);

    auto& body_interface{physics_engine_.body_interface()};

    for (ngnast::scene_graph_t const& scene : model->scenes)
    {
        for (size_t const& root_index : scene.root_indices)
        {
            auto const& root{model->nodes[root_index]};
            glm::vec3 const half_extents{
                (root.aabb.max - root.aabb.min) / 2.0f};

            if (root.name == "Cube")
            {
                assert(root.child_indices.empty());

                if (!root.mesh_index)
                {
                    continue;
                }
                ngnast::mesh_t const& mesh{model->meshes[*root.mesh_index]};

                if (mesh.primitive_indices.size() != 1)
                {
                    assert(false);
                    continue;
                }

                world_primitive_ = model->primitives[mesh.primitive_indices[0]];
                world_aabb_ = root.aabb;

                JPH::MeshShapeSettings const floor_shape_settings{
                    to_jolt_mesh_shape(world_primitive_)};
                floor_shape_settings.SetEmbedded();

                JPH::ShapeSettings::ShapeResult const floor_shape_result{
                    floor_shape_settings.Create()};
                JPH::ShapeRefC const floor_shape{floor_shape_result.Get()};

                JPH::BodyCreationSettings floor_settings{floor_shape,
                    ngnphy::to_jolt(glm::vec3{root.matrix[3]}),
                    ngnphy::to_jolt(glm::quat_cast(root.matrix)),
                    JPH::EMotionType::Static,
                    galileo::object_layers::non_moving};
                floor_settings.mEnhancedInternalEdgeRemoval = true;

                JPH::Body* const floor{
                    body_interface.CreateBody(floor_settings)};

                body_interface.AddBody(floor->GetID(),
                    JPH::EActivation::DontActivate);

                entt::entity const entity{registry_.create()};
                registry_.emplace<component::mesh_t>(entity, root_index);
                registry_.emplace<component::physics_t>(entity, floor->GetID());
            }
            else if (root.name.starts_with("Icosphere"))
            {
                auto sphere_entity{create_sphere(registry_,
                    physics_engine_,
                    scripting_engine_,
                    glm::vec3{root.matrix[3]},
                    half_extents.x)};
                registry_.emplace<component::mesh_t>(sphere_entity, root_index);
            }
            else if (root.name == "Spawn")
            {
                JPH::BodyCreationSettings const spawn_body_settings{
                    new JPH::BoxShapeSettings{
                        {half_extents.x, half_extents.y, half_extents.z}},
                    ngnphy::to_jolt(glm::vec3{root.matrix[3]}),
                    ngnphy::to_jolt(glm::quat_cast(root.matrix)),
                    JPH::EMotionType::Static,
                    galileo::object_layers::non_moving};

                JPH::BodyID const spawner_id{
                    body_interface.CreateAndAddBody(spawn_body_settings,
                        JPH::EActivation::DontActivate)};

                spawner_ = registry_.create();
                body_interface.SetUserData(spawner_id,
                    static_cast<JPH::uint64>(spawner_));

                registry_.emplace<component::mesh_t>(spawner_, root_index);
                registry_.emplace<component::physics_t>(spawner_, spawner_id);
            }
            else if (root.name.starts_with("Character"))
            {
                entt::entity const entity{registry_.create()};
                registry_.emplace<component::mesh_t>(entity, root_index);
                registry_.emplace<component::character_t>(entity);
                character_->set_position(glm::vec3{root.matrix[3]});
            }
        }
    }

    try
    {
        auto const now{std::chrono::system_clock::now()};
        poly_mesh_ =
            generate_poly_mesh(polymesh_params_, world_primitive_, world_aabb_);

        world_.update_navigation_mesh(generate_navigation_mesh(polymesh_params_,
            poly_mesh_,
            world_aabb_));

        auto const diff{std::chrono::system_clock::now() - now};
        spdlog::info("Navigation mesh generation took {}",
            std::chrono::duration_cast<std::chrono::milliseconds>(diff));
    }
    catch (std::exception const& ex)
    {
        spdlog::error(ex.what());
    }
}

void galileo::application_t::spawn_sphere()
{
    std::uniform_real_distribution dist{-25.0f, 25.0f};
    ::galileo::spawn_sphere(registry_,
        physics_engine_,
        scripting_engine_,
        glm::vec3{dist(random_engine_), 10.0f, dist(random_engine_)});
}

bool galileo::application_t::is_spawner(uint32_t const id) const
{
    return spawner_ == static_cast<entt::entity>(id);
}

void galileo::application_t::stop_pathfinding(uint32_t const id)
{
    registry_.remove<component::sphere_path_t>(static_cast<entt::entity>(id));
}
