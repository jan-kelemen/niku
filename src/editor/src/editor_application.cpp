#include <editor_application.hpp>

#include <cppext_container.hpp>
#include <cppext_numeric.hpp>

#include <ngnwsi_imgui_layer.hpp>
#include <ngnwsi_render_window.hpp>
#include <ngnwsi_sdl_window.hpp>

#include <vkrndr_commands.hpp>
#include <vkrndr_cpu_pacing.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_error_code.hpp> // IWYU pragma: keep
#include <vkrndr_execution_port.hpp>
#include <vkrndr_features.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_instance.hpp>
#include <vkrndr_library_handle.hpp>
#include <vkrndr_rendering_context.hpp>
#include <vkrndr_swapchain.hpp>
#include <vkrndr_utility.hpp>

#include <fmt/ranges.h>

#include <imgui.h>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_video.h>

#include <spdlog/common.h>
#include <spdlog/spdlog.h>

#include <volk.h>

#include <vulkan/utility/vk_struct_helper.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <expected>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

// IWYU pragma: no_include <fmt/base.h>
// IWYU pragma: no_include <fmt/format.h>
// IWYU pragma: no_include <boost/smart_ptr/intrusive_ptr.hpp>
// IWYU pragma: no_include <boost/smart_ptr/intrusive_ref_counter.hpp>
// IWYU pragma: no_include <map>
// IWYU pragma: no_include <set>

namespace
{
    constexpr auto application_name{"Niku Editor"};
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
        auto const message{device.error().message()};
        spdlog::error("Failed to create rendering device: {}", message);
        throw std::runtime_error{message};
    }

    vkrndr::execution_port_t& present_queue{
        *std::ranges::find_if(rendering_context_.device->execution_ports,
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

    pools_.reserve(swapchain->frames_in_flight());
    command_buffers_.reserve(swapchain->frames_in_flight());
    for (uint32_t i{}; i != swapchain->frames_in_flight(); ++i)
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
    }
}

editor::application_t::~application_t()
{
    vkDeviceWaitIdle(*rendering_context_.device);

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

bool editor::application_t::handle_event(SDL_Event const& event)
{
    [[maybe_unused]] auto imgui_handled{imgui_->handle_event(event)};

    return event.type != SDL_EVENT_QUIT &&
        event.type != SDL_EVENT_WINDOW_CLOSE_REQUESTED;
}

bool editor::application_t::update()
{
    std::optional<vkrndr::image_t> const target_image{
        main_window_->acquire_next_image()};
    if (!target_image)
    {
        return true;
    }

    uint32_t const index{main_window_->frame_in_flight().index};
    VkCommandBuffer& command_buffer{command_buffers_[index]};

    imgui_->begin_frame();
    ImGui::ShowMetricsWindow();
    imgui_->end_frame();

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

    vkrndr::wait_for_color_attachment_read(*target_image, command_buffer);

    imgui_->render(command_buffer, *target_image);

    vkrndr::transition_to_present_layout(*target_image, command_buffer);

    vkEndCommandBuffer(command_buffer);

    main_window_->present(cppext::as_span(command_buffer));

    return true;
}

editor::application_t::application_t()
{
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
