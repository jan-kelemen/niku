#include <application.hpp>

#include <camera_controller.hpp>
#include <config.hpp>
#include <materials.hpp>
#include <postprocess_shader.hpp>

#include <cppext_container.hpp>
#include <cppext_memory.hpp>
#include <cppext_numeric.hpp>

#include <ngnast_gltf_loader.hpp>
#include <ngnast_gpu_transfer.hpp>

#include <ngngfx_aircraft_camera.hpp>
#include <ngngfx_perspective_projection.hpp>

#include <ngnwsi_application.hpp>
#include <ngnwsi_imgui_layer.hpp>
#include <ngnwsi_mouse.hpp>
#include <ngnwsi_render_window.hpp>
#include <ngnwsi_sdl_window.hpp>

#include <vkglsl_shader_set.hpp>

#include <vkrndr_acceleration_structure.hpp>
#include <vkrndr_backend.hpp>
#include <vkrndr_buffer.hpp>
#include <vkrndr_commands.hpp>
#include <vkrndr_cpu_pacing.hpp>
#include <vkrndr_debug_utils.hpp>
#include <vkrndr_descriptors.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_error_code.hpp>
#include <vkrndr_execution_port.hpp>
#include <vkrndr_features.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_instance.hpp>
#include <vkrndr_library_handle.hpp>
#include <vkrndr_memory.hpp>
#include <vkrndr_pipeline.hpp>
#include <vkrndr_pipeline_layout_builder.hpp>
#include <vkrndr_raytracing_pipeline_builder.hpp>
#include <vkrndr_shader_module.hpp>
#include <vkrndr_swapchain.hpp>
#include <vkrndr_synchronization.hpp>
#include <vkrndr_transient_operation.hpp>
#include <vkrndr_utility.hpp>

#include <boost/scope/defer.hpp>
#include <boost/scope/scope_fail.hpp>

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <glm/matrix.hpp>
#include <glm/vec4.hpp>

#include <imgui.h>

#include <vma_impl.hpp>

#include <volk.h>

#include <vulkan/utility/vk_struct_helper.hpp>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_video.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstring>
#include <exception>
#include <expected>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

// IWYU pragma: no_include <boost/container/deque.hpp>
// IWYU pragma: no_include <boost/intrusive/detail/iterator.hpp>
// IWYU pragma: no_include <boost/smart_ptr/intrusive_ref_counter.hpp>
// IWYU pragma: no_include <boost/scope/exception_checker.hpp>
// IWYU pragma: no_include <fmt/base.h>
// IWYU pragma: no_include <filesystem>
// IWYU pragma: no_include <initializer_list>
// IWYU pragma: no_include <map>
// IWYU pragma: no_include <string>

namespace
{
    struct [[nodiscard]] uniform_data_t final
    {
        glm::mat4 inverse_view;
        glm::mat4 inverse_projection;
        glm::vec4 light_position;
    };

    [[nodiscard]] vkrndr::image_t create_ray_generation_storage_image(
        vkrndr::backend_t const& backend,
        VkExtent2D const extent,
        VkFormat const format)
    {
        return vkrndr::create_image_and_view(backend.device(),
            vkrndr::image_2d_create_info_t{.format = format,
                .extent = extent,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage =
                    VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                .allocation_flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                .required_memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
            VK_IMAGE_ASPECT_COLOR_BIT);
    }

    struct [[nodiscard]] acceleration_structure_primitive_t final
    {
        VkDeviceSize vertices{};
        VkDeviceSize indices{};
        uint32_t count{};
        uint32_t material_index{};
        char padding[32 - 2 * sizeof(VkDeviceSize) - 2 * sizeof(uint32_t)]{};
    };

    static_assert(sizeof(acceleration_structure_primitive_t) == 32);

    acceleration_structure_primitive_t to_bound_primitive(
        ngnast::gpu::primitive_t const& primitive,
        VkDeviceSize const vertex_buffer_address,
        VkDeviceSize const index_buffer_address)
    {
        acceleration_structure_primitive_t rv{
            .vertices = vertex_buffer_address +
                cppext::narrow<VkDeviceSize>(primitive.vertex_offset) *
                    sizeof(ngnast::gpu::vertex_t),
            .count = primitive.count,
            .material_index =
                cppext::narrow<uint32_t>(primitive.material_index),
        };

        if (primitive.is_indexed)
        {
            rv.indices = index_buffer_address +
                cppext::narrow<VkDeviceSize>(primitive.first) *
                    sizeof(uint32_t);
        }

        return rv;
    }

    vkrndr::buffer_t create_primitive_buffer(vkrndr::backend_t& backend,
        ngnast::gpu::acceleration_structure_build_result_t const& build_result)
    {
        vkrndr::buffer_t rv{vkrndr::create_buffer(backend.device(),
            {
                .size = build_result.primitives.size() *
                    sizeof(acceleration_structure_primitive_t),
                .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .required_memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                .alignment = 32,
            })};
        VKRNDR_IF_DEBUG_UTILS(
            object_name(backend.device(), rv, "Primitives Buffer"));

        [[maybe_unused]] boost::scope::scope_fail const destroy_rv{
            [&backend, &rv]() { destroy(backend.device(), rv); }};

        {
            vkrndr::buffer_t staging{
                vkrndr::create_staging_buffer(backend.device(), rv.size)};
            [[maybe_unused]] boost::scope::defer_guard const destroy_staging{
                [&backend, &staging]() { destroy(backend.device(), staging); }};

            vkrndr::mapped_memory_t staging_map{
                vkrndr::map_memory(backend.device(), staging)};
            boost::scope::defer_guard const unmap_staging{
                [&device = backend.device(), &staging_map]()
                { unmap_memory(device, &staging_map); }};
            std::ranges::transform(build_result.primitives,
                staging_map.as<acceleration_structure_primitive_t>(),
                [vb = build_result.vertex_buffer.device_address,
                    ib = build_result.index_buffer.device_address](
                    ngnast::gpu::primitive_t const& primitive)
                { return to_bound_primitive(primitive, vb, ib); });

            backend.transfer_buffer(staging, rv);
        }

        return rv;
    }

    struct [[nodiscard]] push_constants_t final
    {
        VkDeviceSize primitive_buffer_address{};
    };
} // namespace

heatx::application_t::application_t(int argc,
    char const** argv,
    bool const debug)
    : ngnwsi::application_t{ngnwsi::startup_params_t{
          .init_subsystems = {.video = true, .debug = debug},
          .command_line_parameters = {argv, cppext::narrow<size_t>(argc)}}}
    , camera_controller_{camera_, mouse_}
    , render_window_{std::make_unique<ngnwsi::render_window_t>("heatx",
          SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY,
          1280,
          720)}
{
    if (command_line_parameters().size() < 2)
    {
        spdlog::error("Model path not set");
        std::terminate();
    }

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
                        .application_name = "heatx",
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
                        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
                        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
                        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
#if HEATX_SHADER_DEBUG_SYMBOLS
                        VK_KHR_SHADER_RELAXED_EXTENDED_INSTRUCTION_EXTENSION_NAME,
#endif // HEATX_SHADER_DEBUG_SYMBOLS
                    };

                    vkrndr::feature_flags_t feature_flags;
                    vkrndr::add_required_feature_flags(feature_flags);
                    feature_flags.acceleration_structure_flags.push_back(
                        &VkPhysicalDeviceAccelerationStructureFeaturesKHR::
                            accelerationStructure);
                    feature_flags.ray_tracing_pipeline_flags.push_back(
                        &VkPhysicalDeviceRayTracingPipelineFeaturesKHR::
                            rayTracingPipeline);
#if HEATX_SHADER_DEBUG_SYMBOLS
                    feature_flags.relaxed_extended_instruction_flags.emplace(
                        &VkPhysicalDeviceShaderRelaxedExtendedInstructionFeaturesKHR::
                            shaderRelaxedExtendedInstruction);
#endif

                    std::optional<vkrndr::physical_device_features_t> const
                        physical_device{pick_best_physical_device(
                            *rendering_context_.instance,
                            device_extensions,
                            feature_flags,
                            render_window_->create_surface(
                                *rendering_context_.instance))};
                    if (!physical_device)
                    {
                        spdlog::error("No suitable physical device");
                        return std::unexpected{vkrndr::make_error_code(
                            VK_ERROR_INITIALIZATION_FAILED)};
                    }
                    spdlog::info("Selected {} GPU",
                        physical_device->properties.deviceName);

                    vkrndr::feature_chain_t effective_features;
                    set_feature_flags_on_chain(effective_features,
                        feature_flags);
                    link_required_feature_chain(effective_features);

                    if (!vkrndr::enable_extension_for_device(
                            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
                            *physical_device,
                            effective_features))
                    {
                        spdlog::error("Extension {} not available",
                            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
                        return std::unexpected{vkrndr::make_error_code(
                            VK_ERROR_INITIALIZATION_FAILED)};
                    }
                    if (!vkrndr::enable_extension_for_device(
                            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
                            *physical_device,
                            effective_features))
                    {
                        spdlog::error("Extension {} not available",
                            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
                        return std::unexpected{vkrndr::make_error_code(
                            VK_ERROR_INITIALIZATION_FAILED)};
                    }

#if HEATX_SHADER_DEBUG_SYMBOLS
                    if (!vkrndr::enable_extension_for_device(
                            VK_KHR_SHADER_RELAXED_EXTENDED_INSTRUCTION_EXTENSION_NAME,
                            *physical_device,
                            effective_features))
                    {
                        spdlog::error("Extension {} not available",
                            VK_KHR_SHADER_RELAXED_EXTENDED_INSTRUCTION_EXTENSION_NAME);
                        return std::unexpected{vkrndr::make_error_code(
                            VK_ERROR_INITIALIZATION_FAILED)};
                    }
#endif

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
                        effective_features,
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

                    vkrndr::swapchain_t const* const swapchain{
                        render_window_->create_swapchain(
                            *rendering_context_.device,
                            {
                                .preferred_format = VK_FORMAT_R8G8B8A8_UNORM,
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
                })};
    if (!create_result)
    {
        throw std::system_error{create_result.error()};
    }

    camera_.set_position({0.0f, 0.0f, -5.0f});
    camera_.set_yaw_pitch({3.14f, 0.0f});
    projection_.set_near_far_planes({0.1f, 1000.f});
    projection_.set_fov(60.0f);

    raytracing_pipeline_properties_ = vkrndr::get_device_properties<
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR>(
        *rendering_context_.device);
}

heatx::application_t::~application_t() = default;

bool heatx::application_t::should_run()
{
    return static_cast<bool>(render_window_);
}

bool heatx::application_t::handle_event(SDL_Event const& event)
{
    if (event.type == SDL_EVENT_WINDOW_RESIZED)
    {
        on_resize(cppext::narrow<uint32_t>(event.window.data1),
            cppext::narrow<uint32_t>(event.window.data2));
        return true;
    }

    camera_controller_.handle_event(event);

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

    if (event.type == SDL_EVENT_KEY_DOWN)
    {
        auto const& keyboard{event.key};
        if (keyboard.scancode == SDL_SCANCODE_F4)
        {
            imgui_->set_enabled(!imgui_->enabled());
            return true;
        }
    }

    return false;
}

void heatx::application_t::update(float const delta_time)
{
    camera_controller_.update(delta_time);
    projection_.update(camera_.view_matrix());
}

void heatx::application_t::draw()
{
    std::optional<vkrndr::image_t> const target_image{
        render_window_->acquire_next_image()};
    if (!target_image)
    {
        return;
    }

    vkrndr::frame_in_flight_t const& frame{render_window_->frame_in_flight()};

    {
        vkrndr::mapped_memory_t map{vkrndr::map_memory(backend_->device(),
            uniform_buffers_[frame.index])};

        uniform_data_t* const data{map.as<uniform_data_t>()};
        data->inverse_projection =
            glm::inverse(projection_.projection_matrix());
        data->inverse_view = glm::inverse(camera_.view_matrix());
        data->light_position = {light_position_, 0.0f};

        vkrndr::unmap_memory(backend_->device(), &map);
    }

    backend_->begin_frame();

    imgui_->begin_frame();

    camera_controller_.draw_imgui();
    ImGui::ShowMetricsWindow();

    if (ImGui::Begin("Light"))
    {
        ImGui::SliderFloat3("Position",
            glm::value_ptr(light_position_),
            -100.0f,
            100.0f);
    }
    ImGui::End();

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

    push_constants_t const pc{
        .primitive_buffer_address = primitives_.device_address};
    vkCmdPushConstants(command_buffer,
        pipeline_layout_,
        VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
        0,
        sizeof(push_constants_t),
        &pc);

    {
        uint32_t const aligned_handle_size{cppext::aligned_size(
            raytracing_pipeline_properties_.shaderGroupHandleSize,
            raytracing_pipeline_properties_.shaderGroupHandleAlignment)};

        vkrndr::bind_pipeline(command_buffer,
            pipeline_,
            0,
            cppext::as_span(descriptor_sets_[frame.index]));

        materials_->bind_on(command_buffer, pipeline_layout_, pipeline_.type);

        std::array const binding_table_entries{
            VkStridedDeviceAddressRegionKHR{
                .deviceAddress = raygen_binding_table_.device_address,
                .stride = aligned_handle_size,
                .size = aligned_handle_size,
            },
            VkStridedDeviceAddressRegionKHR{
                .deviceAddress = miss_binding_table_.device_address,
                .stride = aligned_handle_size,
                .size = aligned_handle_size,
            },
            VkStridedDeviceAddressRegionKHR{
                .deviceAddress = hit_binding_table_.device_address,
                .stride = aligned_handle_size,
                .size = aligned_handle_size,
            },
        };
        VkStridedDeviceAddressRegionKHR const callable_entry{};

        // NOLINTBEGIN(readability-container-data-pointer)
        vkCmdTraceRaysKHR(command_buffer,
            &binding_table_entries[0],
            &binding_table_entries[1],
            &binding_table_entries[2],
            &callable_entry,
            target_image->extent.width,
            target_image->extent.height,
            1);
        // NOLINTEND(readability-container-data-pointer)
    }

    {
        std::array const image_barriers{
            vkrndr::with_access(
                vkrndr::with_layout(
                    vkrndr::on_stage(vkrndr::image_barrier(*target_image),
                        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT),
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_GENERAL),
                VK_ACCESS_2_NONE,
                VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT),
            vkrndr::with_access(
                vkrndr::with_layout(
                    vkrndr::on_stage(
                        vkrndr::image_barrier(ray_generation_storage_),
                        VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT),
                    VK_IMAGE_LAYOUT_GENERAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
                VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                VK_ACCESS_2_SHADER_SAMPLED_READ_BIT)};

        vkrndr::wait_for(command_buffer, {}, {}, image_barriers);

        postprocess_shader_->draw(command_buffer,
            ray_generation_storage_,
            *target_image);

        VkImageMemoryBarrier2 const revert_storage_barrier{vkrndr::with_access(
            vkrndr::with_layout(
                vkrndr::on_stage(vkrndr::image_barrier(ray_generation_storage_),
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR),
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_IMAGE_LAYOUT_GENERAL),
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT)};
        vkrndr::wait_for(command_buffer,
            {},
            {},
            cppext::as_span(revert_storage_barrier));
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

    vkrndr::transition_to_present_layout(*target_image, command_buffer);

    render_window_->present(backend_->present_buffers());

    backend_->end_frame();
}

void heatx::application_t::end_frame() { imgui_->end_frame(); }

void heatx::application_t::on_startup()
{
    mouse_.set_window_handle(render_window_->platform_window().native_handle());

    materials_ = std::make_unique<materials_t>(*backend_);
    postprocess_shader_ = std::make_unique<postprocess_shader_t>(*backend_);

    ngnast::gltf::loader_t loader;
    if (auto scene{loader.load(command_line_parameters()[1])})
    {
        model_ = ngnast::gpu::build_acceleration_structures(*backend_, *scene);
        primitives_ = create_primitive_buffer(*backend_, model_);
        materials_->load(*scene);
    }

    std::ranges::generate_n(std::back_inserter(uniform_buffers_),
        backend_->frames_in_flight(),
        [this]()
        {
            return vkrndr::create_buffer(backend_->device(),
                {
                    .size = sizeof(uniform_data_t),
                    .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    .allocation_flags =
                        VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
                    .required_memory_flags =
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                });
        });

    std::array const layout_bindings{
        VkDescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        },
        VkDescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
        },
        VkDescriptorSetLayoutBinding{
            .binding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
        },
    };

    if (std::expected<VkDescriptorSetLayout, VkResult> descriptor_layout{
            vkrndr::create_descriptor_set_layout(backend_->device(),
                layout_bindings)})
    {
        descriptor_layout_ = *descriptor_layout;
    }
    else
    {
        throw std::system_error{
            vkrndr::make_error_code(descriptor_layout.error())};
    }

    auto const extent{render_window_->swapchain().extent()};
    on_resize(extent.width, extent.height);

    pipeline_layout_ =
        vkrndr::pipeline_layout_builder_t{backend_->device()}
            .add_descriptor_set_layout(descriptor_layout_)
            .add_descriptor_set_layout(materials_->descriptor_layout())
            .add_push_constants<push_constants_t>(
                VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                VK_SHADER_STAGE_ANY_HIT_BIT_KHR)
            .build();

    vkrndr::raytracing_pipeline_builder_t pipeline_builder{backend_->device(),
        pipeline_layout_};
    pipeline_builder.with_recursion_depth(2);

    std::map<std::string_view, std::pair<vkrndr::shader_module_t, uint32_t>>
        shader_modules;
    for (auto const& [path, stage] :
        {std::make_pair("raygen.rgen", VK_SHADER_STAGE_RAYGEN_BIT_KHR),
            std::make_pair("miss.rmiss", VK_SHADER_STAGE_MISS_BIT_KHR),
            std::make_pair("shadow.rmiss", VK_SHADER_STAGE_MISS_BIT_KHR),
            std::make_pair("closesthit.rchit",
                VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR),
            std::make_pair("anyhit.rahit", VK_SHADER_STAGE_ANY_HIT_BIT_KHR)})
    {
        vkglsl::shader_set_t shader_set{heatx::enable_shader_debug_symbols,
            heatx::enable_shader_optimization};
        if (std::expected<vkrndr::shader_module_t, std::error_code>
                shader_module{vkglsl::add_shader_module_from_path(shader_set,
                    backend_->device(),
                    stage,
                    path)})
        {
            auto [it, inserted] = shader_modules.emplace(path,
                std::make_pair(*std::move(shader_module), uint32_t{}));
            assert(inserted);

            pipeline_builder.add_shader(
                vkrndr::as_pipeline_shader(it->second.first),
                it->second.second);
        }
        else
        {
            spdlog::error("Shader stage {} not loaded {}",
                std::to_underlying(stage),
                shader_module.error().message());
            throw std::system_error{shader_module.error()};
        }
    }

    for (std::string_view name : {"raygen.rgen", "miss.rmiss", "shadow.rmiss"})
    {
        pipeline_builder.add_group(
            vkrndr::general_shader(VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
                shader_modules.at(name).second));
    }

    {
        auto closest_hit_group{vkrndr::closest_hit_shader(
            VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
            shader_modules.at("closesthit.rchit").second)};
        closest_hit_group.anyHitShader =
            shader_modules.at("anyhit.rahit").second;

        pipeline_builder.add_group(closest_hit_group);
    }

    pipeline_ = pipeline_builder.build();

    for (auto const& [name, pair] : shader_modules)
    {
        destroy(backend_->device(), pair.first);
    }

    create_shader_binding_table();
}

void heatx::application_t::on_shutdown()
{
    vkDeviceWaitIdle(backend_->device());

    vkrndr::free_descriptor_sets(backend_->device(),
        descriptor_pool_,
        descriptor_sets_);
    vkrndr::destroy_descriptor_pool(backend_->device(), descriptor_pool_);

    destroy(backend_->device(), hit_binding_table_);
    destroy(backend_->device(), miss_binding_table_);
    destroy(backend_->device(), raygen_binding_table_);

    destroy(backend_->device(), pipeline_);
    destroy(backend_->device(), pipeline_layout_);

    vkDestroyDescriptorSetLayout(backend_->device(),
        descriptor_layout_,
        nullptr);

    for (vkrndr::buffer_t const& buffer : uniform_buffers_)
    {
        destroy(backend_->device(), buffer);
    }

    destroy(backend_->device(), ray_generation_storage_);

    materials_.reset();
    destroy(backend_->device(), primitives_);
    destroy(backend_->device(), model_);

    postprocess_shader_.reset();

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

void heatx::application_t::on_resize(uint32_t const width,
    uint32_t const height)
{
    if (!render_window_->resize(width, height))
    {
        return;
    }

    vkrndr::frame_in_flight_t& in_flight{render_window_->frame_in_flight()};
    std::function<void(std::function<void()>)> const deletion_queue_insert{
        [&in_flight](std::function<void()> cb)
        { in_flight.cleanup.push_back(std::move(cb)); }};

    deletion_queue_insert(
        [&device = backend_->device(), image = ray_generation_storage_]()
        { destroy(device, image); });

    ray_generation_storage_ = create_ray_generation_storage_image(*backend_,
        {width, height},
        VK_FORMAT_R16G16B16A16_SFLOAT);
    VKRNDR_IF_DEBUG_UTILS(object_name(backend_->device(),
        ray_generation_storage_,
        "Ray Generation Storage"));

    {
        vkrndr::transient_operation_t const op{
            backend_->request_transient_operation(false)};

        VkImageMemoryBarrier2 const barrier{vkrndr::with_access(
            vkrndr::on_stage(vkrndr::to_layout(
                                 vkrndr::image_barrier(ray_generation_storage_),
                                 VK_IMAGE_LAYOUT_GENERAL),
                VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT),
            VK_ACCESS_2_NONE,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT)};

        vkrndr::wait_for(op.command_buffer(), {}, {}, cppext::as_span(barrier));
    }

    postprocess_shader_->resize(width, height, deletion_queue_insert);

    deletion_queue_insert([device = &backend_->device(),
                              pool = descriptor_pool_,
                              sets = std::move(descriptor_sets_)]()
        { vkrndr::free_descriptor_sets(*device, pool, sets); });

    if (descriptor_pool_ == VK_NULL_HANDLE)
    {
        create_descriptors();
    }
    else
    {
        allocate_descriptors();

        update_descriptors();
    }

    projection_.set_aspect_ratio(cppext::as_fp(width) / cppext::as_fp(height));
}

void heatx::application_t::create_shader_binding_table()
{
    uint32_t const handle_size{
        raytracing_pipeline_properties_.shaderGroupHandleSize};
    uint32_t const aligned_handle_size{cppext::aligned_size(
        raytracing_pipeline_properties_.shaderGroupHandleSize,
        raytracing_pipeline_properties_.shaderGroupHandleAlignment)};
    uint32_t const group_count{4};
    uint32_t const binding_table_size{group_count * aligned_handle_size};

    std::vector<std::byte> handle_storage;
    handle_storage.resize(binding_table_size);
    if (VkResult const result{
            vkGetRayTracingShaderGroupHandlesKHR(*rendering_context_.device,
                pipeline_,
                0,
                group_count,
                binding_table_size,
                handle_storage.data())};
        !vkrndr::is_success_result(result))
    {
        throw std::system_error{vkrndr::make_error_code(result)};
    }

    auto const create_binding_table_buffer = [this, handle_size]()
    {
        return vkrndr::create_buffer(*rendering_context_.device,
            {
                .size = handle_size,
                .usage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
                    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                .allocation_flags =
                    VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
                .required_memory_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            });
    };

    raygen_binding_table_ = create_binding_table_buffer();
    {
        vkrndr::mapped_memory_t map{
            vkrndr::map_memory(*rendering_context_.device,
                raygen_binding_table_)};

        memcpy(map.as<std::byte>(), handle_storage.data(), handle_size);

        vkrndr::unmap_memory(*rendering_context_.device, &map);
    }

    miss_binding_table_ = create_binding_table_buffer();
    {
        vkrndr::mapped_memory_t map{
            vkrndr::map_memory(*rendering_context_.device,
                miss_binding_table_)};

        memcpy(map.as<std::byte>(),
            handle_storage.data() + aligned_handle_size,
            handle_size * 2);

        vkrndr::unmap_memory(*rendering_context_.device, &map);
    }

    hit_binding_table_ = create_binding_table_buffer();
    {
        vkrndr::mapped_memory_t map{
            vkrndr::map_memory(*rendering_context_.device, hit_binding_table_)};

        memcpy(map.as<std::byte>(),
            handle_storage.data() + size_t{aligned_handle_size} * 3,
            handle_size * 2);

        vkrndr::unmap_memory(*rendering_context_.device, &map);
    }
}

void heatx::application_t::create_descriptors()
{
    std::array const pool_sizes{
        VkDescriptorPoolSize{
            .type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
            .descriptorCount = backend_->frames_in_flight(),
        },
        VkDescriptorPoolSize{
            .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = backend_->frames_in_flight(),
        },
        VkDescriptorPoolSize{
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = backend_->frames_in_flight(),
        },
    };

    if (std::expected<VkDescriptorPool, VkResult> pool{
            vkrndr::create_descriptor_pool(backend_->device(), pool_sizes, 10)})
    {
        descriptor_pool_ = *pool;
    }
    else
    {
        throw std::system_error{vkrndr::make_error_code(pool.error())};
    }

    allocate_descriptors();

    update_descriptors();
}

void heatx::application_t::allocate_descriptors()
{
    descriptor_sets_ = {};

    descriptor_sets_.resize(backend_->frames_in_flight());
    for (uint32_t i{}; i != backend_->frames_in_flight(); ++i)
    {
        if (VkResult const result{
                vkrndr::allocate_descriptor_sets(backend_->device(),
                    descriptor_pool_,
                    cppext::as_span(descriptor_layout_),
                    cppext::as_span(descriptor_sets_[i]))};
            !vkrndr::is_success_result(result))
        {
            throw std::system_error{vkrndr::make_error_code(result)};
        }
    }
}

void heatx::application_t::update_descriptors()
{
    for (uint32_t i{}; i != backend_->frames_in_flight(); ++i)
    {
        VkWriteDescriptorSetAccelerationStructureKHR const
            acceleration_structure_info{
                .sType = vku::GetSType<
                    VkWriteDescriptorSetAccelerationStructureKHR>(),
                .accelerationStructureCount = 1,
                .pAccelerationStructures = &model_.top_level.handle};

        VkWriteDescriptorSet const acceleration_structure_write{
            .sType = vku::GetSType<VkWriteDescriptorSet>(),
            .pNext = &acceleration_structure_info,
            .dstSet = descriptor_sets_[i],
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
        };

        VkDescriptorImageInfo const storage_image_info{
            vkrndr::storage_image_descriptor(ray_generation_storage_)};

        VkWriteDescriptorSet const storage_image_write_write{
            .sType = vku::GetSType<VkWriteDescriptorSet>(),
            .dstSet = descriptor_sets_[i],
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &storage_image_info,
        };

        VkDescriptorBufferInfo const uniform_buffer_info{
            vkrndr::buffer_descriptor(uniform_buffers_[i])};

        VkWriteDescriptorSet const uniform_buffer_write{
            .sType = vku::GetSType<VkWriteDescriptorSet>(),
            .dstSet = descriptor_sets_[i],
            .dstBinding = 2,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &uniform_buffer_info,
        };

        std::array const descriptor_writes{acceleration_structure_write,
            storage_image_write_write,
            uniform_buffer_write};
        vkUpdateDescriptorSets(backend_->device(),
            vkrndr::count_cast(descriptor_writes),
            descriptor_writes.data(),
            0,
            nullptr);
    }
}
