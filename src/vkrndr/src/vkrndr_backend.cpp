#include <vkrndr_backend.hpp>

#include <vkrndr_buffer.hpp>
#include <vkrndr_commands.hpp>
#include <vkrndr_descriptors.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_execution_port.hpp>
#include <vkrndr_features.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_instance.hpp>
#include <vkrndr_library.hpp>
#include <vkrndr_memory.hpp>
#include <vkrndr_render_settings.hpp>
#include <vkrndr_swapchain.hpp>
#include <vkrndr_transient_operation.hpp>
#include <vkrndr_utility.hpp>
#include <vkrndr_window.hpp>

#include <cppext_container.hpp>
#include <cppext_cycled_buffer.hpp>

#include <boost/scope/defer.hpp>
#include <boost/scope/scope_fail.hpp>

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <expected>
#include <iterator>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

// IWYU pragma: no_include <boost/scope/exception_checker.hpp>
// IWYU pragma: no_include <fmt/base.h>
// IWYU pragma: no_include <initializer_list>
// IWYU pragma: no_include <functional>
// IWYU pragma: no_include <unordered_map>
// IWYU pragma: no_include <map>

namespace
{
    constexpr std::array const required_device_extensions{
        VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkDescriptorPool create_descriptor_pool(vkrndr::device_t& device)
    {
        auto pool{vkrndr::create_descriptor_pool(device,
            std::to_array<VkDescriptorPoolSize>({
                // clang-format off
                {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1000},
                {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1000},
                {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 1000},
                {.type = VK_DESCRIPTOR_TYPE_SAMPLER, .descriptorCount = 1000},
                {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1000},
                {.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = 1000},
                // clang-format on
            }),
            1000,
            VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT)};

        if (!pool)
        {
            vkrndr::check_result(pool.error());
        }

        return std::move(pool).value();
    }

    [[nodiscard]] std::vector<vkrndr::physical_device_features_t>
    filter_physical_devices(vkrndr::instance_t const& instance,
        vkrndr::feature_flags_t const& required_feature_flags,
        VkSurfaceKHR surface)
    {
        std::vector<vkrndr::physical_device_features_t> rv;

        for (vkrndr::physical_device_features_t& device :
            vkrndr::query_available_physical_devices(instance.handle, surface))
        {
            bool const device_extensions_supported{
                std::ranges::all_of(required_device_extensions,
                    [de = device.extensions](std::string_view name)
                    {
                        return std::ranges::contains(de,
                            name,
                            &VkExtensionProperties::extensionName);
                    })};
            if (!device_extensions_supported)
            {
                continue;
            }

            if (!vkrndr::check_feature_flags(device.features,
                    required_feature_flags))
            {
                continue;
            }

            if (surface != VK_NULL_HANDLE)
            {
                bool const has_present_queue{std::ranges::any_of(
                    device.queue_families,
                    [](vkrndr::queue_family_t const& family)
                    {
                        return family.supports_present &&
                            vkrndr::supports_flags(family.properties.queueFlags,
                                VK_QUEUE_GRAPHICS_BIT);
                    })};
                if (!has_present_queue)
                {
                    continue;
                }

                if (!device.swapchain_support ||
                    device.swapchain_support->surface_formats.empty() ||
                    device.swapchain_support->present_modes.empty())
                {
                    continue;
                }
            }

            rv.emplace_back(std::move(device));
        }

        return rv;
    }

    [[nodiscard]] auto pick_device_by_type(
        std::ranges::forward_range auto&& devices)
    {
        for (auto it{std::begin(devices)}; it != std::end(devices); ++it)
        {
            if (it->properties.deviceType ==
                VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            {
                return it;
            }
        }

        for (auto it{std::begin(devices)}; it != std::end(devices); ++it)
        {
            if (it->properties.deviceType ==
                VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
            {
                return it;
            }
        }

        for (auto it{std::begin(devices)}; it != std::end(devices); ++it)
        {
            if (it->properties.deviceType ==
                VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU)
            {
                return it;
            }
        }

        return std::end(devices);
    }
} // namespace

vkrndr::backend_t::backend_t(std::shared_ptr<library_handle_t>&& library,
    std::vector<char const*> const& instance_extensions,
    std::function<VkSurfaceKHR(VkInstance)> get_surface,
    uint32_t frames_in_flight,
    bool const debug)
    : library_{std::move(library)}
{
    auto required_instance_extensions{instance_extensions};

#if VKRNDR_ENABLE_DEBUG_UTILS
    if (debug &&
        is_instance_extension_available(*library_,
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
    {
        required_instance_extensions.push_back(
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
#endif

    {
        auto const capabilities2{is_instance_extension_available(*library_,
            VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME)};

        auto const maintenance1{is_instance_extension_available(*library_,
            VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME)};

        if (capabilities2 && maintenance1)
        {
            required_instance_extensions.push_back(
                VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
            required_instance_extensions.push_back(
                VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
        }
    }

    instance_create_info_t const instance_create_info{
        .maximum_vulkan_version = VK_API_VERSION_1_3,
        .extensions = required_instance_extensions};
    instance_ = create_instance(instance_create_info);

    spdlog::info(
        "Created Vulkan instance {}.\n\tEnabled extensions: {}\n\tEnabled layers: {}",
        std::bit_cast<intptr_t>(instance_.handle),
        fmt::join(instance_.extensions, ", "),
        fmt::join(instance_.layers, ", "));

    feature_flags_t required_flags;
    add_required_feature_flags(required_flags);

    auto const physical_devices{filter_physical_devices(instance_,
        required_flags,
        get_surface(instance_.handle))};
    auto physical_device_it{pick_device_by_type(physical_devices)};
    if (physical_device_it == std::end(physical_devices))
    {
        throw std::runtime_error{"Suitable physical device not found"};
    }

    queue_family_t const family{*std::ranges::find_if(
        physical_device_it->queue_families,
        [](queue_family_t const& f)
        {
            return f.supports_present &&
                supports_flags(f.properties.queueFlags, VK_QUEUE_GRAPHICS_BIT);
        })};

    std::vector<char const*> effective_extensions{
        std::cbegin(required_device_extensions),
        std::cend(required_device_extensions)};

    feature_chain_t effective_features;
    set_feature_flags_on_chain(effective_features, required_flags);
    link_required_feature_chain(effective_features);

    if (enable_extension_for_device(
            VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME,
            *physical_device_it,
            effective_features))
    {
        effective_extensions.push_back(
            VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME);
    }

    if (enable_extension_for_device(VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME,
            *physical_device_it,
            effective_features))
    {
        effective_extensions.push_back(VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME);
    }

    if (enable_extension_for_device(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
            *physical_device_it,
            effective_features))
    {
        effective_extensions.push_back(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
    }

    device_create_info_t const device_create_info{
        .chain = &effective_features.device_10_features,
        .device = physical_device_it->device,
        .extensions = effective_extensions,
        .queues = cppext::as_span(family)};
    device_ = create_device(instance_, device_create_info);

    spdlog::info("Created Vulkan device {}.\n\tEnabled extensions: {}",
        std::bit_cast<intptr_t>(device_.logical),
        fmt::join(device_.extensions, ", "));

    auto& port{device_.execution_ports.emplace_back(device_.logical,
        family.properties.queueFlags,
        family.index,
        0)};
    device_.present_queue = &port;
    device_.transfer_queue = &port;

    frame_data_ = cppext::cycled_buffer_t<frame_data_t>{frames_in_flight,
        frames_in_flight};
    descriptor_pool_ = ::create_descriptor_pool(device_);

    for (frame_data_t& fd : cppext::as_span(frame_data_))
    {
        fd.present_queue = device_.present_queue;
        fd.present_command_pool =
            create_command_pool(device_, fd.present_queue->queue_family())
                .value();
        fd.present_transient_command_pool = create_command_pool(device_,
            fd.present_queue->queue_family(),
            VK_COMMAND_POOL_CREATE_TRANSIENT_BIT)
                                                .value();

        fd.transfer_queue = device_.transfer_queue;
        fd.transfer_transient_command_pool =
            create_command_pool(device_, fd.transfer_queue->queue_family())
                .value();
    };
}

vkrndr::backend_t::~backend_t()
{
    for (frame_data_t& fd : cppext::as_span(frame_data_))
    {
        destroy_command_pool(device_, fd.present_command_pool);
        destroy_command_pool(device_, fd.present_transient_command_pool);
        destroy_command_pool(device_, fd.transfer_transient_command_pool);
    };

    destroy_descriptor_pool(device_, descriptor_pool_);

    destroy(&device_);

    destroy(&instance_);
}

vkrndr::instance_t& vkrndr::backend_t::instance() noexcept { return instance_; }

vkrndr::instance_t const& vkrndr::backend_t::instance() const noexcept
{
    return instance_;
}

vkrndr::device_t& vkrndr::backend_t::device() noexcept { return device_; }

vkrndr::device_t const& vkrndr::backend_t::device() const noexcept
{
    return device_;
}

VkDescriptorPool vkrndr::backend_t::descriptor_pool()
{
    return descriptor_pool_;
}

uint32_t vkrndr::backend_t::frames_in_flight() const
{
    return cppext::narrow<uint32_t>(frame_data_.size());
}

void vkrndr::backend_t::begin_frame()
{
    check_result(
        reset_command_pool(device_, frame_data_->present_command_pool));
    check_result(reset_command_pool(device_,
        frame_data_->transfer_transient_command_pool));
}

std::span<VkCommandBuffer const> vkrndr::backend_t::present_buffers()
{
    std::span const rv{frame_data_->present_command_buffers.data(),
        frame_data_->used_present_command_buffers};

    for (VkCommandBuffer const buffer : rv)
    {
        check_result(vkEndCommandBuffer(buffer));
    }

    return rv;
}

void vkrndr::backend_t::end_frame()
{
    frame_data_.cycle([](frame_data_t& fd, frame_data_t const&)
        { fd.used_present_command_buffers = 0; });
}

VkCommandBuffer vkrndr::backend_t::request_command_buffer()
{
    if (frame_data_->used_present_command_buffers ==
        frame_data_->present_command_buffers.size())
    {
        frame_data_->present_command_buffers.resize(
            frame_data_->present_command_buffers.size() + 1);

        check_result(allocate_command_buffers(device_,
            frame_data_->present_command_pool,
            true,
            1,
            cppext::as_span(frame_data_->present_command_buffers.back())));
    }

    VkCommandBuffer rv{frame_data_->present_command_buffers[frame_data_
            ->used_present_command_buffers++]};

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    check_result(vkBeginCommandBuffer(rv, &begin_info));
    return rv;
}

vkrndr::transient_operation_t vkrndr::backend_t::request_transient_operation(
    bool const transfer_only)
{
    if (transfer_only)
    {
        return {device_,
            *frame_data_->transfer_queue,
            frame_data_->transfer_transient_command_pool};
    }

    return {device_,
        *frame_data_->present_queue,
        frame_data_->present_transient_command_pool};
}

vkrndr::image_t vkrndr::backend_t::transfer_image(
    std::span<std::byte const> const& image_data,
    VkExtent2D const extent,
    VkFormat const format,
    uint32_t const mip_levels)
{
    buffer_t staging_buffer{create_staging_buffer(device_, image_data.size())};
    boost::scope::defer_guard const rollback{[this, staging_buffer]() mutable
        { destroy(&device_, &staging_buffer); }};

    {
        mapped_memory_t staging_map{map_memory(device_, staging_buffer)};
        memcpy(staging_map.mapped_memory, image_data.data(), image_data.size());
        unmap_memory(device_, &staging_map);
    }

    return transfer_buffer_to_image(staging_buffer, extent, format, mip_levels);
}

vkrndr::image_t vkrndr::backend_t::transfer_buffer_to_image(
    vkrndr::buffer_t const& source,
    VkExtent2D const extent,
    VkFormat const format,
    uint32_t const mip_levels)
{
    image_t image{create_image_and_view(device_,
        image_2d_create_info_t{.format = format,
            .extent = extent,
            .mip_levels = mip_levels,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                VK_IMAGE_USAGE_SAMPLED_BIT,
            .required_memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
        VK_IMAGE_ASPECT_COLOR_BIT)};
    boost::scope::scope_fail const rollback{
        [this, &image]() mutable { destroy(&device_, &image); }};

    {
        auto transient{request_transient_operation(false)};
        VkCommandBuffer cb{transient.command_buffer()};
        wait_for_transfer_write(image.image, cb, mip_levels);
        copy_buffer_to_image(cb, source.buffer, image.image, extent);
        if (mip_levels == 1)
        {
            wait_for_transfer_write_completed(image.image, cb, mip_levels);
        }
        else
        {
            generate_mipmaps(device_,
                image.image,
                cb,
                format,
                extent,
                mip_levels);
        }
    }

    return image;
}

void vkrndr::backend_t::transfer_buffer(buffer_t const& source,
    buffer_t const& target)
{
    auto transient{request_transient_operation(false)};
    copy_buffer_to_buffer(transient.command_buffer(),
        source.buffer,
        source.size,
        target.buffer);
}
