#include <application.hpp>
#include <config.hpp>

#include <cppext_container.hpp>
#include <cppext_memory.hpp>

#include <ngnwsi_application.hpp>
#include <ngnwsi_imgui_layer.hpp>
#include <ngnwsi_render_window.hpp>
#include <ngnwsi_sdl_window.hpp>

#include <ngngfx_aircraft_camera.hpp>
#include <ngngfx_perspective_projection.hpp>

#include <vkglsl_shader_set.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_commands.hpp>
#include <vkrndr_cpu_pacing.hpp>
#include <vkrndr_debug_utils.hpp>
#include <vkrndr_descriptors.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_error_code.hpp>
#include <vkrndr_execution_port.hpp>
#include <vkrndr_features.hpp>
#include <vkrndr_instance.hpp>
#include <vkrndr_library_handle.hpp>
#include <vkrndr_memory.hpp>
#include <vkrndr_pipeline.hpp>
#include <vkrndr_pipeline_layout_builder.hpp>
#include <vkrndr_raytracing_pipeline_builder.hpp>

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <imgui.h>

#include <volk.h>

#include <vulkan/utility/vk_struct_helper.hpp>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <exception>
#include <expected>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

// IWYU pragma: no_include <boost/smart_ptr/intrusive_ref_counter.hpp>
// IWYU pragma: no_include <fmt/base.h>
// IWYU pragma: no_include <filesystem>
// IWYU pragma: no_include <functional>
// IWYU pragma: no_include <map>
// IWYU pragma: no_include <string>

namespace
{
    struct [[nodiscard]] uniform_data_t final
    {
        glm::mat4 inverse_view;
        glm::mat4 inverse_projection;
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
                .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                    VK_IMAGE_USAGE_STORAGE_BIT,
                .allocation_flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                .required_memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
            VK_IMAGE_ASPECT_COLOR_BIT);
    }
} // namespace

heatx::application_t::application_t(bool const debug)
    : ngnwsi::application_t{ngnwsi::startup_params_t{
          .init_subsystems = {.video = true, .debug = debug}}}
    , render_window_{std::make_unique<ngnwsi::render_window_t>("heatx",
          SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY,
          1280,
          720)}
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
                        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME};

                    vkrndr::feature_flags_t feature_flags;
                    vkrndr::add_required_feature_flags(feature_flags);
                    feature_flags.acceleration_structure_flags.push_back(
                        &VkPhysicalDeviceAccelerationStructureFeaturesKHR::
                            accelerationStructure);
                    feature_flags.ray_tracing_pipeline_flags.push_back(
                        &VkPhysicalDeviceRayTracingPipelineFeaturesKHR::
                            rayTracingPipeline);

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

                    vkrndr::swapchain_t* const swapchain{
                        render_window_->create_swapchain(
                            *rendering_context_.device,
                            {
                                .preferred_format = VK_FORMAT_B8G8R8A8_UNORM,
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

    return false;
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

    backend_->begin_frame();

    imgui_->begin_frame();

    ImGui::ShowMetricsWindow();

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
        uint32_t const aligned_handle_size{cppext::aligned_size(
            raytracing_pipeline_properties_.shaderGroupHandleSize,
            raytracing_pipeline_properties_.shaderGroupHandleAlignment)};

        vkrndr::bind_pipeline(command_buffer,
            pipeline_,
            0,
            cppext::as_span(descriptor_sets_[frame.index]));

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
        VkStridedDeviceAddressRegionKHR callable_entry{};

        vkCmdTraceRaysKHR(command_buffer,
            &binding_table_entries[0],
            &binding_table_entries[1],
            &binding_table_entries[2],
            &callable_entry,
            target_image->extent.width,
            target_image->extent.height,
            1);
    }

    {
        std::array const image_barriers{
            vkrndr::with_access(
                vkrndr::with_layout(
                    vkrndr::on_stage(vkrndr::image_barrier(*target_image),
                        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                        VK_PIPELINE_STAGE_2_TRANSFER_BIT),
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
                VK_ACCESS_2_NONE,
                VK_ACCESS_2_TRANSFER_WRITE_BIT),
            vkrndr::with_access(
                vkrndr::with_layout(
                    vkrndr::on_stage(
                        vkrndr::image_barrier(ray_generation_storage_),
                        VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
                        VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR),
                    VK_IMAGE_LAYOUT_GENERAL,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
                VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                VK_ACCESS_2_TRANSFER_READ_BIT)};

        vkrndr::wait_for(command_buffer, {}, {}, image_barriers);

        VkImageCopy const region{
            .srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
            .dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
            .extent = target_image->extent,
        };

        vkCmdCopyImage(command_buffer,
            ray_generation_storage_,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            *target_image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region);

        VkImageMemoryBarrier2 const revert_storage_barrier{vkrndr::with_access(
            vkrndr::with_layout(
                vkrndr::on_stage(vkrndr::image_barrier(ray_generation_storage_),
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR),
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_IMAGE_LAYOUT_GENERAL),
            VK_ACCESS_2_TRANSFER_READ_BIT,
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
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT),
                VK_ACCESS_2_TRANSFER_WRITE_BIT,
                VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)};
        vkrndr::wait_for(command_buffer, {}, {}, cppext::as_span(barrier));

        imgui_->render(command_buffer, *target_image);
    }

    vkrndr::transition_to_present_layout(target_image->image, command_buffer);

    render_window_->present(backend_->present_buffers());

    backend_->end_frame();
}

void heatx::application_t::end_frame() { }

void heatx::application_t::on_startup()
{
    create_blas();
    create_tlas();

    glm::mat4 view = glm::lookAtRH(glm::vec3{0.0f, 0.0f, -5.0f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f});

    glm::mat4 projection =
        glm::perspectiveRH_ZO(1.047198f, 1280.0f / 720, 0.1f, 1000.0f);

    std::ranges::generate_n(std::back_inserter(uniform_buffers_),
        backend_->frames_in_flight(),
        [this, &view, &projection]()
        {
            vkrndr::buffer_t rv{vkrndr::create_buffer(backend_->device(),
                {
                    .size = sizeof(uniform_data_t),
                    .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    .allocation_flags =
                        VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
                    .required_memory_flags =
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                })};

            vkrndr::mapped_memory_t map{
                vkrndr::map_memory(backend_->device(), rv)};

            map.as<uniform_data_t>()->inverse_projection = projection;
            map.as<uniform_data_t>()->inverse_view = view;

            vkrndr::unmap_memory(backend_->device(), &map);

            return rv;
        });

    std::array const layout_bindings{
        VkDescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
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
            .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
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

    vkrndr::raytracing_pipeline_builder_t pipeline_builder{backend_->device(),
        vkrndr::pipeline_layout_builder_t{backend_->device()}
            .add_descriptor_set_layout(descriptor_layout_)
            .build()};

    uint32_t raygen_stage{};
    uint32_t miss_stage{};
    uint32_t closest_hit_stage{};

    std::array<vkrndr::shader_module_t, 3> mods;

    auto it{std::begin(mods)};

    vkglsl::shader_set_t shader_set{heatx::enable_shader_debug_symbols,
        heatx::enable_shader_optimization};
    for (auto const& [path, stage] :
        {std::make_pair("raygen.rgen", VK_SHADER_STAGE_RAYGEN_BIT_KHR),
            std::make_pair("miss.rmiss", VK_SHADER_STAGE_MISS_BIT_KHR),
            std::make_pair("closesthit.rchit",
                VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)})
    {
        auto& stage_idx{[&raygen_stage, &miss_stage, &closest_hit_stage](
                            VkShaderStageFlagBits const s) -> uint32_t&
            {
                if (s == VK_SHADER_STAGE_RAYGEN_BIT_KHR)
                {
                    return raygen_stage;
                }
                else if (s == VK_SHADER_STAGE_MISS_BIT_KHR)
                {
                    return miss_stage;
                }
                return closest_hit_stage;
            }(stage)};

        if (std::expected<vkrndr::shader_module_t, std::error_code>
                shader_module{vkglsl::add_shader_module_from_path(shader_set,
                    backend_->device(),
                    stage,
                    path)})
        {
            *it = *std::move(shader_module);

            pipeline_builder.add_shader(vkrndr::as_pipeline_shader(*it),
                stage_idx);

            ++it;
        }
        else
        {
            spdlog::error("Shader stage {} not loaded {}",
                std::to_underlying(stage),
                shader_module.error().message());
            throw std::system_error{shader_module.error()};
        }
    }

    pipeline_builder.add_group(
        vkrndr::general_shader(VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            raygen_stage));
    pipeline_builder.add_group(
        vkrndr::general_shader(VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            miss_stage));
    pipeline_builder.add_group(vkrndr::closest_hit_shader(
        VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
        closest_hit_stage));

    pipeline_ = pipeline_builder.build();

    for (auto const& shd : mods)
    {
        destroy(&backend_->device(), &shd);
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

    destroy(&backend_->device(), &hit_binding_table_);
    destroy(&backend_->device(), &miss_binding_table_);
    destroy(&backend_->device(), &raygen_binding_table_);

    destroy(&backend_->device(), &pipeline_);

    vkDestroyDescriptorSetLayout(backend_->device(),
        descriptor_layout_,
        nullptr);

    for (vkrndr::buffer_t const& buffer : uniform_buffers_)
    {
        destroy(&backend_->device(), &buffer);
    }

    destroy(&backend_->device(), &ray_generation_storage_);

    destroy(backend_->device(), tlas_);
    destroy(backend_->device(), blas_);

    destroy(&backend_->device(), &instance_buffer_);
    destroy(&backend_->device(), &transform_buffer_);
    destroy(&backend_->device(), &index_buffer_);
    destroy(&backend_->device(), &vertex_buffer_);

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
    std::function<void(std::function<void()>)> deletion_queue_insert{
        [&in_flight](std::function<void()> cb)
        { in_flight.cleanup.push_back(std::move(cb)); }};

    deletion_queue_insert([device = &backend_->device(),
                              image = std::move(ray_generation_storage_)]()
        { destroy(device, &image); });

    ray_generation_storage_ = create_ray_generation_storage_image(*backend_,
        {width, height},
        render_window_->swapchain().image_format());
    VKRNDR_IF_DEBUG_UTILS(object_name(backend_->device(),
        ray_generation_storage_,
        "Ray Generation Storage"));

    {
        vkrndr::transient_operation_t op{
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
}

void heatx::application_t::create_blas()
{
    vertex_buffer_ = vkrndr::create_buffer(*rendering_context_.device,
        {
            .size = sizeof(glm::vec3) * 3,
            .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .required_memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        });

    index_buffer_ = vkrndr::create_buffer(*rendering_context_.device,
        {
            .size = sizeof(uint32_t) * 3,
            .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .required_memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        });

    transform_buffer_ = vkrndr::create_buffer(*rendering_context_.device,
        {
            .size = sizeof(VkTransformMatrixKHR),
            .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .required_memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        });

    VkAccelerationStructureGeometryKHR const acceleration_structure_geometry{
        .sType = vku::GetSType<VkAccelerationStructureGeometryKHR>(),
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry =
            {.triangles =
                    {
                        .sType = vku::GetSType<
                            VkAccelerationStructureGeometryTrianglesDataKHR>(),
                        .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
                        .vertexData = vertex_buffer_.device_address,
                        .vertexStride = sizeof(glm::vec3),
                        .maxVertex = 3,
                        .indexType = VK_INDEX_TYPE_UINT32,
                        .indexData = index_buffer_.device_address,
                        .transformData = transform_buffer_.device_address,
                    }},
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
    };

    VkAccelerationStructureBuildGeometryInfoKHR
        acceleration_structure_build_geometry{
            .sType =
                vku::GetSType<VkAccelerationStructureBuildGeometryInfoKHR>(),
            .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
            .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
            .geometryCount = 1,
            .pGeometries = &acceleration_structure_geometry,
        };

    uint32_t const triangles{1};
    auto acceleration_structure_build_sizes{
        vku::InitStruct<VkAccelerationStructureBuildSizesInfoKHR>()};
    vkGetAccelerationStructureBuildSizesKHR(*rendering_context_.device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &acceleration_structure_build_geometry,
        &triangles,
        &acceleration_structure_build_sizes);

    blas_ = vkrndr::create_acceleration_structure(*rendering_context_.device,
        VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        acceleration_structure_build_sizes);

    vkrndr::buffer_t const scratch_buffer{
        vkrndr::create_scratch_buffer(*rendering_context_.device,
            acceleration_structure_build_sizes)};

    acceleration_structure_build_geometry.mode =
        VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    acceleration_structure_build_geometry.dstAccelerationStructure =
        blas_.handle;
    acceleration_structure_build_geometry.scratchData.deviceAddress =
        scratch_buffer.device_address;

    VkAccelerationStructureBuildRangeInfoKHR const build_range_info{
        .primitiveCount = triangles,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0};
    auto const range_infos{std::to_array({&build_range_info})};

    vkrndr::buffer_t const staging{
        vkrndr::create_staging_buffer(*rendering_context_.device,
            vertex_buffer_.size + index_buffer_.size + transform_buffer_.size)};
    {
        vkrndr::transient_operation_t transient{
            backend_->request_transient_operation(false)};

        VkCommandBuffer cb{transient.command_buffer()};

        vkrndr::mapped_memory_t map{
            vkrndr::map_memory(*rendering_context_.device, staging)};
        {
            glm::vec3* const vertices{map.as<glm::vec3>()};
            vertices[0] = {1.0f, 1.0f, 0.0f};
            vertices[1] = {-1.0f, 1.0f, 0.0f};
            vertices[2] = {0.0f, -1.0f, 0.0f};

            VkBufferCopy const copy_vertex{
                .srcOffset = 0,
                .dstOffset = 0,
                .size = vertex_buffer_.size,
            };
            vkCmdCopyBuffer(cb, staging, vertex_buffer_, 1, &copy_vertex);
        }

        {
            uint32_t* const indices{map.as<uint32_t>(vertex_buffer_.size)};
            indices[0] = 0;
            indices[1] = 1;
            indices[2] = 2;

            VkBufferCopy const copy_index{
                .srcOffset = vertex_buffer_.size,
                .dstOffset = 0,
                .size = index_buffer_.size,
            };
            vkCmdCopyBuffer(cb, staging, index_buffer_, 1, &copy_index);
        }

        {
            VkTransformMatrixKHR* const transforms{map.as<VkTransformMatrixKHR>(
                vertex_buffer_.size + index_buffer_.size)};
            // clang-format off
            *transforms = 
            {
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
            };
            // clang-format on

            VkBufferCopy const copy_transform{
                .srcOffset = vertex_buffer_.size + index_buffer_.size,
                .dstOffset = 0,
                .size = transform_buffer_.size,
            };
            vkCmdCopyBuffer(cb, staging, transform_buffer_, 1, &copy_transform);
        }
        vkrndr::unmap_memory(*rendering_context_.device, &map);

        auto buffer_barriers{std::to_array({
            vkrndr::buffer_barrier(vertex_buffer_),
            vkrndr::buffer_barrier(index_buffer_),
            vkrndr::buffer_barrier(transform_buffer_),
        })};
        std::ranges::transform(buffer_barriers,
            std::begin(buffer_barriers),
            [](auto const& barrier)
            {
                return vkrndr::with_access(
                    vkrndr::on_stage(barrier,
                        VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,
                        VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR),
                    VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    VK_ACCESS_2_SHADER_READ_BIT);
            });

        vkrndr::wait_for(cb, {}, buffer_barriers, {});

        vkCmdBuildAccelerationStructuresKHR(cb,
            1,
            &acceleration_structure_build_geometry,
            range_infos.data());

        auto build_wait{vkrndr::with_access(
            vkrndr::on_stage(vkrndr::buffer_barrier(blas_.buffer),
                VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR),
            VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
            VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR)};

        vkrndr::wait_for(cb, {}, cppext::as_span(build_wait), {});
    }
    destroy(rendering_context_.device.get(), &staging);
    destroy(rendering_context_.device.get(), &scratch_buffer);

    blas_.device_address = device_address(*rendering_context_.device, blas_);
}

void heatx::application_t::create_tlas()
{
    instance_buffer_ = vkrndr::create_buffer(*rendering_context_.device,
        {
            .size = sizeof(VkAccelerationStructureInstanceKHR),
            .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .allocation_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
            .required_memory_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        });
    {
        VkAccelerationStructureInstanceKHR const instance{
            .transform = {1.0f,
                0.0f,
                0.0f,
                0.0f,
                0.0f,
                1.0f,
                0.0f,
                0.0f,
                0.0f,
                0.0f,
                1.0f,
                0.0f},
            .mask = 0xFF,
            .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
            .accelerationStructureReference = blas_.device_address,
        };

        vkrndr::mapped_memory_t map{
            vkrndr::map_memory(backend_->device(), instance_buffer_)};

        memcpy(map.as<VkAccelerationStructureInstanceKHR>(),
            &instance,
            sizeof(instance));

        vkrndr::unmap_memory(backend_->device(), &map);
    }

    VkAccelerationStructureGeometryKHR const acceleration_structure_geometry{
        .sType = vku::GetSType<VkAccelerationStructureGeometryKHR>(),
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry =
            {
                .instances =
                    {
                        .sType = vku::GetSType<
                            VkAccelerationStructureGeometryInstancesDataKHR>(),
                        .data =
                            {
                                .deviceAddress =
                                    instance_buffer_.device_address,
                            },
                    },
            },
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
    };

    VkAccelerationStructureBuildGeometryInfoKHR
        acceleration_structure_build_geometry{
            .sType =
                vku::GetSType<VkAccelerationStructureBuildGeometryInfoKHR>(),
            .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
            .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
            .geometryCount = 1,
            .pGeometries = &acceleration_structure_geometry,
        };

    uint32_t const primitives{1};
    auto acceleration_structure_build_sizes{
        vku::InitStruct<VkAccelerationStructureBuildSizesInfoKHR>()};
    vkGetAccelerationStructureBuildSizesKHR(*rendering_context_.device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &acceleration_structure_build_geometry,
        &primitives,
        &acceleration_structure_build_sizes);

    tlas_ = vkrndr::create_acceleration_structure(*rendering_context_.device,
        VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        acceleration_structure_build_sizes);

    vkrndr::buffer_t const scratch_buffer{
        vkrndr::create_scratch_buffer(*rendering_context_.device,
            acceleration_structure_build_sizes)};

    acceleration_structure_build_geometry.mode =
        VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    acceleration_structure_build_geometry.dstAccelerationStructure =
        tlas_.handle;
    acceleration_structure_build_geometry.scratchData.deviceAddress =
        scratch_buffer.device_address;

    VkAccelerationStructureBuildRangeInfoKHR const build_range_info{
        .primitiveCount = primitives,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0};
    auto const range_infos{std::to_array({&build_range_info})};

    {
        vkrndr::transient_operation_t transient{
            backend_->request_transient_operation(false)};

        VkCommandBuffer cb{transient.command_buffer()};

        vkCmdBuildAccelerationStructuresKHR(cb,
            1,
            &acceleration_structure_build_geometry,
            range_infos.data());

        auto build_wait{vkrndr::with_access(
            vkrndr::on_stage(vkrndr::buffer_barrier(tlas_.buffer),
                VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT),
            VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
            VK_ACCESS_2_NONE)};

        vkrndr::wait_for(cb, {}, cppext::as_span(build_wait), {});
    }
    destroy(rendering_context_.device.get(), &scratch_buffer);

    tlas_.device_address = device_address(*rendering_context_.device, tlas_);
}

void heatx::application_t::create_shader_binding_table()
{
    uint32_t const handle_size{
        raytracing_pipeline_properties_.shaderGroupHandleSize};
    uint32_t const aligned_handle_size{cppext::aligned_size(
        raytracing_pipeline_properties_.shaderGroupHandleSize,
        raytracing_pipeline_properties_.shaderGroupHandleAlignment)};
    uint32_t const group_count{3};
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
            handle_size);

        vkrndr::unmap_memory(*rendering_context_.device, &map);
    }

    hit_binding_table_ = create_binding_table_buffer();
    {
        vkrndr::mapped_memory_t map{
            vkrndr::map_memory(*rendering_context_.device, hit_binding_table_)};

        memcpy(map.as<std::byte>(),
            handle_storage.data() + aligned_handle_size * 2,
            handle_size);

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
                .pAccelerationStructures = &tlas_.handle};

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
            vkrndr::count_cast(descriptor_writes.size()),
            descriptor_writes.data(),
            0,
            nullptr);
    }
}
