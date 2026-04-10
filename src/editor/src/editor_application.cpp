#include <editor_application.hpp>

#include <material_manager.hpp>

#include <camera_controller.hpp>
#include <grid_shader.hpp>

#include <cppext_container.hpp>
#include <cppext_numeric.hpp>

#include <ngnast_gltf_loader.hpp>
#include <ngnast_gpu_transfer.hpp>
#include <ngnast_scene_model.hpp>

#include <ngngfx_aircraft_camera.hpp>
#include <ngngfx_perspective_projection.hpp>

#include <ngnwsi_fixed_timestep.hpp>
#include <ngnwsi_imgui_layer.hpp>
#include <ngnwsi_render_window.hpp>
#include <ngnwsi_sdl_window.hpp>

#include <vkrndr_buffer.hpp>
#include <vkrndr_commands.hpp>
#include <vkrndr_cpu_pacing.hpp>
#include <vkrndr_debug_utils.hpp>
#include <vkrndr_descriptors.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_error_code.hpp> // IWYU pragma: keep
#include <vkrndr_execution_port.hpp>
#include <vkrndr_features.hpp>
#include <vkrndr_formats.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_instance.hpp>
#include <vkrndr_library_handle.hpp>
#include <vkrndr_memory.hpp>
#include <vkrndr_pipeline.hpp>
#include <vkrndr_render_pass.hpp>
#include <vkrndr_rendering_context.hpp>
#include <vkrndr_swapchain.hpp>
#include <vkrndr_synchronization.hpp>
#include <vkrndr_utility.hpp>

#include <entt/signal/sigh.hpp>

#include <fmt/ranges.h>
#include <fmt/std.h> // IWYU pragma: keep

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <imgui.h>

#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_video.h>

#include <spdlog/common.h>
#include <spdlog/spdlog.h>

#include <tracy_impl.hpp>

#include <volk.h>

#include <vma_impl.hpp>

#include <vulkan/utility/vk_struct_helper.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

// IWYU pragma: no_include <boost/core/allocator_access.hpp>
// IWYU pragma: no_include <boost/smart_ptr/intrusive_ptr.hpp>
// IWYU pragma: no_include <boost/smart_ptr/intrusive_ref_counter.hpp>
// IWYU pragma: no_include <fmt/base.h>
// IWYU pragma: no_include <fmt/format.h>
// IWYU pragma: no_include <SDL3/SDL_begin_code.h>
// IWYU pragma: no_include <map>
// IWYU pragma: no_include <set>

namespace
{
    constexpr auto application_name{"Niku Editor"};

    struct [[nodiscard]] frame_info_t final
    {
        glm::mat4 view;
        glm::mat4 projection;
        glm::vec3 position;
    };

    [[nodiscard]] std::expected<VkDescriptorPool, std::error_code>
    create_descriptor_pool(vkrndr::device_t const& device)
    {
        return create_descriptor_pool(device,
            std::to_array<VkDescriptorPoolSize>({
                // clang-format off
                {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1000},
                // clang-format on
            }),
            1000,
            VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT)
            .transform_error(vkrndr::make_error_code);
    }

    [[nodiscard]] std::expected<VkDescriptorSetLayout, std::error_code>
    create_frame_info_descriptor_layout(vkrndr::device_t const& device)
    {
        static constexpr VkDescriptorSetLayoutBinding position_binding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags =
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        };

        return create_descriptor_set_layout(device,
            cppext::as_span(position_binding))
            .transform_error(vkrndr::make_error_code);
    }

    void update_frame_info_descriptor_set(vkrndr::device_t const& device,
        VkDescriptorSet const descriptor_set,
        vkrndr::buffer_t const& buffer)
    {
        VkDescriptorBufferInfo const buffer_info{
            vkrndr::buffer_descriptor(buffer)};

        VkWriteDescriptorSet const write_info{
            .sType = vku::GetSType<VkWriteDescriptorSet>(),
            .dstSet = descriptor_set,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &buffer_info,
        };

        vkUpdateDescriptorSets(device, 1, &write_info, 0, nullptr);
    }

    constexpr std::array<SDL_DialogFileFilter, 1> const filters{
        {{"Asset files", "gltf;glb"}}};

    void SDLCALL file_open_callback(void* const userdata,
        char const* const* const filelist,
        [[maybe_unused]] int const filter)
    {
        if (!filelist)
        {
            spdlog::error("Error during execution of file open callback: {}",
                SDL_GetError());
            return;
        }

        if (!*filelist)
        {
            spdlog::trace("File open callback was cancelled");
            return;
        }

        char const* const* it{filelist};
        size_t count{};
        while (*it)
        {
            ++count;
            ++it;
        }

        spdlog::info("Selected files: {}", count);
        static_cast<editor::application_t*>(userdata)->load_files(
            {filelist, count});
    }
} // namespace

editor::application_t::application_t(
    std::span<char const*> command_line_parameters)
    : application_t()
{
    process_command_line(command_line_parameters);

    main_window_ = std::make_unique<ngnwsi::render_window_t>(application_name,
        SDL_WINDOW_MAXIMIZED | SDL_WINDOW_RESIZABLE,
        1920,
        1080);

    // Create Vulkan instance
    std::vector<char const*> const instance_extensions{
        ngnwsi::sdl_window_t::required_extensions()};
    if (std::expected<vkrndr::instance_ptr_t, std::error_code> instance{
            vkrndr::create_instance({
                .extensions = instance_extensions,
                .application_name = application_name,
            })})
    {
        spdlog::debug(
            "Created with instance handle {}.\n\tEnabled extensions: {}\n\tEnabled layers: {}",
            vkrndr::handle_cast((*instance)->handle),
            fmt::join((*instance)->extensions, ", "),
            fmt::join((*instance)->layers, ", "));

        rendering_context_.instance = *std::move(instance);
    }
    else
    {
        auto const message{instance.error().message()};
        spdlog::error("Failed to create rendering instance: {}", message);
        throw std::runtime_error{message};
    }

    // Pick a device that has suitable features
    static constexpr std::array const device_extensions{
        VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    std::optional<vkrndr::physical_device_features_t> const physical_device{
        pick_best_physical_device(*rendering_context_.instance,
            device_extensions,
            main_window_->create_surface(*rendering_context_.instance))};
    if (!physical_device)
    {
        static constexpr auto message{"No suitable physical device"};
        spdlog::error(message);
        throw std::runtime_error{message};
    }
    spdlog::info("Selected {} GPU", physical_device->properties.deviceName);

    // Pick the rendering queue that is able to present
    auto const queue_with_present{
        std::ranges::find_if(physical_device->queue_families,
            [](vkrndr::queue_family_t const& f)
            {
                return f.supports_present &&
                    vkrndr::supports_flags(f.properties.queueFlags,
                        VK_QUEUE_GRAPHICS_BIT);
            })};
    if (queue_with_present == cend(physical_device->queue_families))
    {
        static constexpr auto message{"No present queue"};
        spdlog::error(message);
        throw std::runtime_error{message};
    }

    // Create the Vulkan device
    if (std::expected<vkrndr::device_ptr_t, std::error_code> device{
            create_device(*rendering_context_.instance,
                device_extensions,
                *physical_device,
                cppext::as_span(*queue_with_present))})
    {
        spdlog::debug(
            "Created with device handle {}.\n\tEnabled extensions: {}",
            vkrndr::handle_cast((*device)->logical_device),
            fmt::join((*device)->extensions, ", "));

        rendering_context_.device = *std::move(device);
    }
    else
    {
        auto const& message{device.error().message()};
        spdlog::error("Failed to create rendering device: {}", message);
        throw std::runtime_error{message};
    }

    std::vector<VkFormat> const formats{
        vkrndr::find_supported_texture_compression_formats(
            rendering_context_.device->physical_device)};
    gltf_loader_ = std::make_unique<ngnast::gltf::loader_t>(std::span{formats});

    vkrndr::execution_port_t& present_queue{
        **std::ranges::find_if(rendering_context_.device->execution_ports,
            &vkrndr::execution_port_t::has_present)};

    vkrndr::swapchain_t const* const swapchain{
        main_window_->create_swapchain(*rendering_context_.device,
            {
                .preferred_format = VK_FORMAT_B8G8R8A8_UNORM,
                .image_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                    VK_IMAGE_USAGE_STORAGE_BIT,
                .preferred_present_mode = VK_PRESENT_MODE_FIFO_KHR,
                .present_queue = &present_queue,
            })};

    imgui_ =
        std::make_unique<ngnwsi::imgui_layer_t>(main_window_->platform_window(),
            *rendering_context_.instance,
            *rendering_context_.device,
            *swapchain,
            present_queue);

    if (std::expected<VkDescriptorPool, std::error_code> const result{
            create_descriptor_pool(*rendering_context_.device)})
    {
        descriptor_pool_ = *result;
        VKRNDR_IF_DEBUG_UTILS(object_name(*rendering_context_.device,
            VK_OBJECT_TYPE_DESCRIPTOR_POOL,
            vkrndr::handle_cast(*result),
            "Global Descriptor Pool"));
    }
    else
    {
        auto const& message{result.error().message()};
        spdlog::error("Failed to create frame descriptor set layout: {}",
            message);
        throw std::runtime_error{message};
    }

    if (std::expected<VkDescriptorSetLayout, std::error_code> const result{
            create_frame_info_descriptor_layout(*rendering_context_.device)})
    {
        frame_info_layout_ = *result;
        VKRNDR_IF_DEBUG_UTILS(object_name(*rendering_context_.device,
            VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
            vkrndr::handle_cast(*result),
            "Frame Descriptor Set Layout"));
    }
    else
    {
        auto const& message{result.error().message()};
        spdlog::error("Failed to create frame descriptor set layout: {}",
            message);
        throw std::runtime_error{message};
    }

    uint32_t const frames_in_flight{swapchain->frames_in_flight()};
    pools_.reserve(frames_in_flight);
    command_buffers_.reserve(frames_in_flight);
    frame_info_descriptors_.reserve(frames_in_flight);
    frame_info_buffers_.reserve(frames_in_flight);

    for (uint32_t i{}; i != frames_in_flight; ++i)
    {
        if (std::expected<VkCommandPool, std::error_code> pool{
                create_command_pool(*rendering_context_.device,
                    present_queue.queue_family(),
                    VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT)})
        {
            pools_.push_back(*pool);
        }
        else
        {
            auto const& message{pool.error().message()};
            spdlog::error("Failed to create command pool for frame {}: {}",
                i,
                message);
            throw std::runtime_error{message};
        }

        VkCommandBuffer current_buffer{VK_NULL_HANDLE};
        if (std::expected<void, std::error_code> const result{
                vkrndr::allocate_command_buffers(*rendering_context_.device,
                    pools_[i],
                    true,
                    cppext::as_span(current_buffer))})
        {
            command_buffers_.push_back(current_buffer);
        }
        else
        {
            auto const& message{result.error().message()};
            spdlog::error("Failed to create command buffer for frame {}: {}",
                i,
                message);
            throw std::runtime_error{message};
        }

        if (VkResult const result{
                vkrndr::allocate_descriptor_sets(*rendering_context_.device,
                    descriptor_pool_,
                    cppext::as_span(frame_info_layout_),
                    cppext::as_span(
                        frame_info_descriptors_.emplace_back(VK_NULL_HANDLE)))};
            !vkrndr::is_success_result(result))
        {
            auto const& message{vkrndr::make_error_code(result).message()};
            spdlog::error("Failed to allocate descriptor set for frame {}: {}",
                i,
                message);
            throw std::runtime_error{message};
        }

        frame_info_buffers_.push_back(create_buffer(*rendering_context_.device,
            {.size = sizeof(frame_info_t),
                .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                .allocation_flags =
                    VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
                .required_memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT}));

        update_frame_info_descriptor_set(*rendering_context_.device,
            frame_info_descriptors_.back(),
            frame_info_buffers_.back());
    }

    vkrndr::fence_pool_t fence_pool_{*rendering_context_.device};

    auto const execute_transfer =
        [this, &present_queue, &fence_pool_](
            std::function<void(VkCommandBuffer)> const& callback)
        -> std::expected<void, std::error_code>
    {
        std::expected<void, std::error_code> rv;

        VkCommandBuffer command_buffer{VK_NULL_HANDLE};
        auto cb_span{cppext::as_span(command_buffer)};

        rv = {begin_single_time_commands(*rendering_context_.device,
            pools_.front(),
            cb_span)};
        if (!rv)
        {
            return rv;
        }

        callback(command_buffer);

        if (std::expected<VkFence, VkResult> const fence{fence_pool_.get()})
        {
            rv = end_single_time_commands(present_queue, cb_span, *fence);
            if (rv)
            {
                if (VkResult const result{
                        vkWaitForFences(*rendering_context_.device,
                            1,
                            &fence.value(),
                            VK_TRUE,
                            std::numeric_limits<uint64_t>::max())};
                    !vkrndr::is_success_result(result))
                {
                    rv = std::unexpected{vkrndr::make_error_code(result)};
                }
            }

            if (VkResult const result{fence_pool_.recycle(*fence)};
                !vkrndr::is_success_result(result))
            {
                rv = std::unexpected{vkrndr::make_error_code(result)};
            }
        }
        else
        {
            rv = std::unexpected{vkrndr::make_error_code(fence.error())};
        }

        free_command_buffers(*rendering_context_.device,
            pools_.front(),
            cb_span);

        return rv;
    };

    if (std::expected<grid_shader_t, std::error_code> grid_shader_result{
            create_grid_shader(*rendering_context_.device,
                swapchain->image_format(),
                execute_transfer)})
    {
        grid_shader_ = std::make_unique<grid_shader_t>(
            std::move(grid_shader_result).value());
    }
    else
    {
        auto const& message{grid_shader_result.error().message()};
        spdlog::error("Failed to create grid shader: {}", message);
        throw std::runtime_error{message};
    }

    if (std::expected<material_manager_t, std::error_code>
            material_manager_result{
                create_material_manager(*rendering_context_.device)})
    {
        material_manager_ = std::move(material_manager_result).value();
    }
    else
    {
        auto const& message{material_manager_result.error().message()};
        spdlog::error("Failed to create grid shader: {}", message);
        throw std::runtime_error{message};
    }

    event_dispatcher_.sink<SDL_Event>()
        .connect<&ngnwsi::imgui_layer_t::handle_event>(*imgui_);
    event_dispatcher_.sink<SDL_Event>()
        .connect<&camera_controller_t::handle_event>(camera_controller_);
    event_dispatcher_.sink<SDL_Event>().connect<&application_t::handle_event>(
        *this);
    event_dispatcher_.sink<main_thread_callback_t>()
        .connect<&application_t::execute_main_thread_callbacks>(*this);

    render_thread_ = std::make_unique<std::jthread>(
        [this](std::stop_token const& token)
        {
            while (!token.stop_requested())
            {
                render();
            }
        });
}

editor::application_t::~application_t()
{
    render_thread_.reset();

    vkDeviceWaitIdle(*rendering_context_.device);

    if (grid_shader_)
    {
        destroy(*rendering_context_.device, *grid_shader_);
    }

    destroy(*rendering_context_.device, material_manager_);

    std::ranges::for_each(frame_info_buffers_,
        [&d = *rendering_context_.device](vkrndr::buffer_t const& b)
        { destroy(d, b); });

    std::ranges::for_each(frame_info_descriptors_,
        [this, &d = *rendering_context_.device](VkDescriptorSet const s)
        {
            vkrndr::free_descriptor_sets(d,
                descriptor_pool_,
                cppext::as_span(s));
        });

    vkDestroyDescriptorSetLayout(*rendering_context_.device,
        frame_info_layout_,
        nullptr);

    vkDestroyDescriptorPool(*rendering_context_.device,
        descriptor_pool_,
        nullptr);

    command_buffers_.clear();

    std::ranges::for_each(pools_,
        [&d = *rendering_context_.device](VkCommandPool const cp)
        { destroy_command_pool(d, cp); });

    imgui_.reset();

    if (main_window_)
    {
        main_window_->destroy_swapchain();
        main_window_->destroy_surface(*rendering_context_.instance);
        main_window_.reset();
    }

    destroy(rendering_context_);

    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

bool editor::application_t::update()
{
    ZoneScoped;

    {
        std::unique_lock guard{state_mutex_};
        event_dispatcher_.update();
    }

    if (uint64_t const steps{timestep_.pending_simulation_steps()})
    {
        std::unique_lock guard{state_mutex_};
        for (uint64_t i{}; i != steps; ++i)
        {
            camera_controller_.update(timestep_.update_interval);
        }
        projection_.update(camera_.view_matrix());
    }

    return continue_running_;
}

void editor::application_t::load_files(
    std::span<char const* const> const& file_list)
{
    thread_pool_.detach_task(
        [this,
            paths = std::vector<std::filesystem::path>{begin(file_list),
                end(file_list)}]()
        {
            ZoneScoped;
            std::vector<std::tuple<ngnast::scene_model_t,
                ngnast::gpu::geometry_transfer_result_t>>
                loaded_models;
            loaded_models.reserve(paths.size());

            std::ranges::for_each(paths,
                [this, &loaded_models](std::filesystem::path const& p)
                {
                    if (std::expected<ngnast::scene_model_t, std::system_error>
                            load_result{gltf_loader_->load(p)})
                    {
                        if (std::expected<std::vector<size_t>, std::error_code>
                                samplers_result{add_samplers(material_manager_,
                                    *rendering_context_.device,
                                    load_result->samplers)})
                        {
                            spdlog::info(
                                "Loaded samplers for model {}, indices {}",
                                p,
                                fmt::join(*samplers_result, ", "));

                            ngnast::gpu::geometry_transfer_result_t
                                transfer_result{ngnast::gpu::transfer_geometry(
                                    *rendering_context_.device,
                                    *load_result)};

                            loaded_models.emplace_back(
                                std::move(load_result).value(),
                                std::move(transfer_result));
                            spdlog::info("Model loaded '{}'", p);
                        }
                        else
                        {
                            auto const& message{
                                samplers_result.error().message()};
                            spdlog::error(
                                "Failed to register samplers for model {}: {}",
                                p,
                                message);
                        }
                    }
                    else
                    {
                        spdlog::error("Failed to load model '{}': ",
                            load_result.error());
                    }
                });

            // TODO-JK: Temporary code
            std::ranges::for_each(loaded_models,
                [this](decltype(loaded_models)::value_type const& v)
                { destroy(*rendering_context_.device, std::get<1>(v)); });
        });
}

editor::application_t::application_t() : camera_controller_{camera_, mouse_}
{
    camera_.set_position({0.0f, 2.0f, 1.0f});
    camera_.set_yaw_pitch({0.0f, 1.0f});
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        throw std::runtime_error{SDL_GetError()};
    }

    if (std::expected<vkrndr::library_handle_ptr_t, std::error_code> lh{
            vkrndr::initialize()})
    {
        rendering_context_.library_handle = *std::move(lh);
    }
    else
    {
        auto const message{lh.error().message()};
        spdlog::error("Failed to load rendering library: {}", message);
        throw std::runtime_error{message};
    }
}

void editor::application_t::process_command_line(
    std::span<char const*> const& parameters)
{
    auto const has_argument = [&parameters](std::string_view s)
    { return std::ranges::contains(cbegin(parameters), cend(parameters), s); };

    if (has_argument("--trace"))
    {
        spdlog::set_level(spdlog::level::trace);
    }
    else if (has_argument("--debug"))
    {
        spdlog::set_level(spdlog::level::debug);
    }
}

void editor::application_t::handle_event(SDL_Event const& event)
{
    if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
    {
        render_thread_->request_stop();
        continue_running_ = false;
    }
    else if (event.type == SDL_EVENT_QUIT)
    {
        continue_running_ = false;
    }
}

void editor::application_t::execute_main_thread_callbacks(
    main_thread_callback_t& cb)
{
    cb.callback();
}

void editor::application_t::render()
{
    std::optional<vkrndr::image_t> const target_image{
        main_window_->acquire_next_image(std::numeric_limits<uint64_t>::max())};
    if (!target_image)
    {
        return;
    }

    ZoneScoped;

    uint32_t const index{main_window_->frame_in_flight().index};

    {
        std::shared_lock guard{state_mutex_};

        vkrndr::mapped_memory_t map{
            map_memory(*rendering_context_.device, frame_info_buffers_[index])};
        *map.as<frame_info_t>() = {.view = camera_.view_matrix(),
            .projection = projection_.projection_matrix(),
            .position = camera_.position()};
        unmap_memory(*rendering_context_.device, &map);
    }

    imgui_->begin_frame();
    ImGui::DockSpaceOverViewport(0,
        ImGui::GetMainViewport(),
        ImGuiDockNodeFlags_PassthruCentralNode);

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open"))
            {
                entt::delegate const delegate{
                    +[]([[maybe_unused]] void const* instance)
                    {
                        SDL_ShowOpenFileDialog(file_open_callback,
                            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
                            const_cast<void*>(instance),
                            nullptr,
                            filters.data(),
                            cppext::narrow<int>(filters.size()),
                            nullptr,
                            false);
                    },
                    this};

                event_dispatcher_.enqueue<main_thread_callback_t>(delegate);
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Quit"))
            {
                event_dispatcher_.enqueue<SDL_Event>(
                    {.quit = {.type = SDL_EVENT_QUIT}});
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    ImGui::ShowMetricsWindow();

    ImGui::ShowDemoWindow();

    camera_controller_.draw_imgui();

    imgui_->end_frame();

    VkCommandBuffer& command_buffer{command_buffers_[index]};
    VkCommandBufferBeginInfo const begin_info{
        .sType = vku::GetSType<VkCommandBufferBeginInfo>(),
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    if (VkResult const result{
            vkBeginCommandBuffer(command_buffer, &begin_info)};
        !vkrndr::is_success_result(result))
    {
        auto const message{vkrndr::make_error_code(result).message()};
        spdlog::error("Failed to begin command buffer: {}", message);
        throw std::runtime_error{message};
    }

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
        auto const barrier{vkrndr::with_layout(
            vkrndr::with_access(
                vkrndr::on_stage(image_barrier(*target_image),
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT),
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT),
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)};
        vkrndr::wait_for(command_buffer, {}, {}, cppext::as_span(barrier));
    }

    {
        vkrndr::render_pass_t grid_color_pass;
        grid_color_pass.with_color_attachment(VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            target_image->view,
            VkClearValue{.color = {{0.0f, 0.0f, 0.0f, 0.0f}}});

        vkCmdBindDescriptorSets(command_buffer,
            grid_shader_->pipeline.type,
            grid_shader_->pipeline_layout,
            0,
            1,
            &frame_info_descriptors_[index],
            0,
            nullptr);

        vkrndr::render_pass_guard_t guard{grid_color_pass.begin(command_buffer,
            VkRect2D{.offset = {0, 0},
                .extent = vkrndr::to_2d_extent(target_image->extent)})};
        draw(*grid_shader_, command_buffer);
    }

    {
        auto const barrier{vkrndr::with_access(
            vkrndr::on_stage(image_barrier(*target_image),
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT),
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT)};
        vkrndr::wait_for(command_buffer, {}, {}, cppext::as_span(barrier));
    }
    vkrndr::wait_for_color_attachment_read(*target_image, command_buffer);

    imgui_->render(command_buffer, *target_image);

    vkrndr::transition_to_present_layout(*target_image, command_buffer);

    vkEndCommandBuffer(command_buffer);

    main_window_->present(cppext::as_span(command_buffer));

    FrameMark;
}
