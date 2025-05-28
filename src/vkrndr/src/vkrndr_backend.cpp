#include <vkrndr_backend.hpp>

#include <vkrndr_buffer.hpp>
#include <vkrndr_command_pool.hpp>
#include <vkrndr_commands.hpp>
#include <vkrndr_context.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_execution_port.hpp>
#include <vkrndr_features.hpp>
#include <vkrndr_global_data.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_memory.hpp>
#include <vkrndr_render_settings.hpp>
#include <vkrndr_swap_chain.hpp>
#include <vkrndr_transient_operation.hpp>
#include <vkrndr_utility.hpp>
#include <vkrndr_window.hpp>

#include <cppext_container.hpp>
#include <cppext_cycled_buffer.hpp>

#include <boost/scope/defer.hpp>
#include <boost/scope/scope_fail.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <iterator>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

// IWYU pragma: no_include <boost/scope/exception_checker.hpp>
// IWYU pragma: no_include <initializer_list>
// IWYU pragma: no_include <functional>
// IWYU pragma: no_include <unordered_map>

namespace
{
    constexpr std::array const required_device_extensions{
        VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    struct [[nodiscard]] feature_chain_t final
    {
        VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT
            swapchain_maintenance_1_features{
                .sType =
                    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT};

        VkPhysicalDeviceVulkan13Features required_device_13_features{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};

        VkPhysicalDeviceVulkan12Features required_device_12_features{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};

        VkPhysicalDeviceFeatures2 required_device_features{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    };

    void link_all(feature_chain_t& chain)
    {
        chain.required_device_features.pNext =
            &chain.required_device_12_features;
        chain.required_device_12_features.pNext =
            &chain.required_device_13_features;
        chain.required_device_13_features.pNext =
            &chain.swapchain_maintenance_1_features;
    }

    void link_required(feature_chain_t& chain)
    {
        chain.required_device_features.pNext =
            &chain.required_device_12_features;
        chain.required_device_12_features.pNext =
            &chain.required_device_13_features;
        chain.required_device_13_features.pNext = nullptr;
    }

    void set_required_features(feature_chain_t& chain)
    {
        chain.required_device_features.features.independentBlend = VK_TRUE;
        chain.required_device_features.features.sampleRateShading = VK_TRUE;
        chain.required_device_features.features.wideLines = VK_TRUE;
        chain.required_device_features.features.samplerAnisotropy = VK_TRUE;
        chain.required_device_features.features.tessellationShader = VK_TRUE;

        chain.required_device_12_features
            .shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
        chain.required_device_12_features.runtimeDescriptorArray = VK_TRUE;

        chain.required_device_13_features.synchronization2 = VK_TRUE;
        chain.required_device_13_features.dynamicRendering = VK_TRUE;
    }

    VkDescriptorPool create_descriptor_pool(vkrndr::device_t const& device)
    {
        VkDescriptorPoolSize uniform_buffer_pool_size{};
        uniform_buffer_pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uniform_buffer_pool_size.descriptorCount = 1000;

        VkDescriptorPoolSize storage_buffer_pool_size{};
        storage_buffer_pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        storage_buffer_pool_size.descriptorCount = 1000;

        VkDescriptorPoolSize storage_image_pool_size{};
        storage_image_pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        storage_image_pool_size.descriptorCount = 1000;

        VkDescriptorPoolSize sampler_pool_size{};
        sampler_pool_size.type = VK_DESCRIPTOR_TYPE_SAMPLER;
        sampler_pool_size.descriptorCount = 1000;

        VkDescriptorPoolSize combined_sampler_pool_size{};
        combined_sampler_pool_size.type =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        combined_sampler_pool_size.descriptorCount = 1000;

        VkDescriptorPoolSize sampled_image_pool_size{};
        sampled_image_pool_size.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        sampled_image_pool_size.descriptorCount = 1000;

        std::array pool_sizes{uniform_buffer_pool_size,
            storage_buffer_pool_size,
            storage_image_pool_size,
            sampler_pool_size,
            combined_sampler_pool_size,
            sampled_image_pool_size};

        VkDescriptorPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.poolSizeCount = vkrndr::count_cast(pool_sizes.size());
        pool_info.pPoolSizes = pool_sizes.data();
        pool_info.maxSets = 1000;

        VkDescriptorPool rv{};
        vkrndr::check_result(
            vkCreateDescriptorPool(device.logical, &pool_info, nullptr, &rv));

        return rv;
    }

    [[nodiscard]] std::optional<feature_chain_t> check_physical_device_features(
        VkPhysicalDevice device)
    {
        feature_chain_t rv;
        link_all(rv);

        vkGetPhysicalDeviceFeatures2(device, &rv.required_device_features);

        auto const all_true = [](std::ranges::range auto&& range)
        {
            return std::all_of(std::begin(range),
                std::end(range),
                [](auto const value) { return value == VK_TRUE; });
        };

        std::array const f10{
            rv.required_device_features.features.independentBlend,
            rv.required_device_features.features.sampleRateShading,
            rv.required_device_features.features.wideLines,
            rv.required_device_features.features.samplerAnisotropy,
            rv.required_device_features.features.tessellationShader};
        if (!all_true(f10))
        {
            return {};
        }

        std::array const f12{rv.required_device_12_features
                                 .shaderSampledImageArrayNonUniformIndexing,
            rv.required_device_12_features.runtimeDescriptorArray};
        if (!all_true(f12))
        {
            return {};
        }

        std::array const f13{rv.required_device_13_features.synchronization2,
            rv.required_device_13_features.dynamicRendering};
        if (!all_true(f13))
        {
            return {};
        }

        return rv;
    }

    struct [[nodiscard]] queried_physical_device_t final
    {
        VkPhysicalDevice device;
        VkPhysicalDeviceProperties properties;
        std::vector<VkExtensionProperties> extensions;
        std::vector<vkrndr::queue_family_t> queue_families;
        vkrndr::swap_chain_support_t swap_chain_support;
        feature_chain_t optional_features;
    };

    [[nodiscard]] std::vector<queried_physical_device_t>
    filter_physical_devices(vkrndr::context_t const& context,
        VkSurfaceKHR surface)
    {
        std::vector<queried_physical_device_t> rv;
        for (VkPhysicalDevice device :
            vkrndr::query_physical_devices(context.instance))
        {
            queried_physical_device_t qpd{.device = device};

            qpd.extensions = vkrndr::query_device_extensions(device);
            bool const device_extensions_supported{
                std::ranges::all_of(required_device_extensions,
                    [de = qpd.extensions](std::string_view name)
                    {
                        return std::ranges::contains(de,
                            name,
                            &VkExtensionProperties::extensionName);
                    })};

            if (!device_extensions_supported)
            {
                continue;
            }

            if (auto optional_features{check_physical_device_features(device)})
            {
                qpd.optional_features = *optional_features;
            }

            qpd.queue_families = vkrndr::query_queue_families(device, surface);
            if (surface != VK_NULL_HANDLE)
            {
                bool const has_present_queue{std::ranges::any_of(
                    qpd.queue_families,
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

                qpd.swap_chain_support =
                    vkrndr::query_swap_chain_support(device, surface);
                if (qpd.swap_chain_support.surface_formats.empty() ||
                    qpd.swap_chain_support.present_modes.empty())
                {
                    continue;
                }
            }

            vkGetPhysicalDeviceProperties(device, &qpd.properties);

            rv.emplace_back(std::move(qpd));
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

vkrndr::backend_t::backend_t(window_t& window,
    render_settings_t const& settings,
    bool const debug)
    : render_settings_{settings}
    , window_{&window}
{
    auto required_instance_extensions{window_->required_extensions()};

    check_result(volkInitialize());

    auto const instance_extensions{query_instance_extensions()};
#if VKRNDR_ENABLE_DEBUG_UTILS
    if (debug)
    {
        auto const debug_utils{std::ranges::contains(instance_extensions,
            std::string_view{VK_EXT_DEBUG_UTILS_EXTENSION_NAME},
            &VkExtensionProperties::extensionName)};
        if (debug_utils)
        {
            required_instance_extensions.push_back(
                VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
    }
#endif

    {
        auto const capabilities2{std::ranges::contains(instance_extensions,
            std::string_view{VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME},
            &VkExtensionProperties::extensionName)};

        auto const maintenance1{std::ranges::contains(instance_extensions,
            std::string_view{VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME},
            &VkExtensionProperties::extensionName)};

        if (capabilities2 && maintenance1)
        {
            required_instance_extensions.push_back(
                VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
            required_instance_extensions.push_back(
                VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
        }
    }

    context_create_info_t const context_create_info{
        .minimal_vulkan_version = VK_API_VERSION_1_3,
        .extensions = required_instance_extensions};
    context_ = create_context(context_create_info);

    auto const physical_devices{filter_physical_devices(context_,
        window_->create_surface(context_.instance))};
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
    set_required_features(effective_features);
    link_required(effective_features);
    if (physical_device_it->optional_features.swapchain_maintenance_1_features
            .swapchainMaintenance1 == VK_TRUE)
    {
        assert(std::ranges::contains(physical_device_it->extensions,
            std::string_view{VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME},
            &VkExtensionProperties::extensionName));
        effective_extensions.push_back(
            VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME);

        effective_features.swapchain_maintenance_1_features
            .swapchainMaintenance1 = VK_TRUE;

        effective_features.swapchain_maintenance_1_features.pNext =
            effective_features.required_device_13_features.pNext;
        effective_features.required_device_13_features.pNext =
            &effective_features.swapchain_maintenance_1_features;

        render_settings_.swapchain_maintenance_1_supported = true;
    }

    device_create_info_t const device_create_info{
        .chain = &effective_features.required_device_features,
        .device = physical_device_it->device,
        .extensions = effective_extensions,
        .queues = cppext::as_span(family)};
    device_ = create_device(context_, device_create_info);

    auto& port{device_.execution_ports.emplace_back(device_.logical,
        family.properties.queueFlags,
        family.index,
        0)};
    device_.present_queue = &port;
    device_.transfer_queue = &port;

    swap_chain_ =
        std::make_unique<swap_chain_t>(*window_, device_, render_settings_);
    frame_data_ =
        cppext::cycled_buffer_t<frame_data_t>{settings.frames_in_flight,
            settings.frames_in_flight};
    descriptor_pool_ = create_descriptor_pool(device_);

    for (frame_data_t& fd : cppext::as_span(frame_data_))
    {
        fd.present_queue = device_.present_queue;
        fd.present_command_pool = std::make_unique<command_pool_t>(device_,
            fd.present_queue->queue_family());
        fd.present_transient_command_pool =
            std::make_unique<command_pool_t>(device_,
                fd.present_queue->queue_family(),
                VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);

        fd.transfer_queue = device_.transfer_queue;
        fd.transfer_transient_command_pool =
            std::make_unique<command_pool_t>(device_,
                fd.transfer_queue->queue_family());
    };
}

vkrndr::backend_t::~backend_t()
{
    for (frame_data_t& fd : cppext::as_span(frame_data_))
    {
        fd.present_command_pool.reset();
        fd.present_transient_command_pool.reset();
        fd.transfer_transient_command_pool.reset();
    };

    vkDestroyDescriptorPool(device_.logical, descriptor_pool_, nullptr);

    swap_chain_.reset();

    destroy(&device_);

    window_->destroy_surface(context_.instance);

    destroy(&context_);

    volkFinalize();
}

VkFormat vkrndr::backend_t::image_format() const
{
    return swap_chain_->image_format();
}

uint32_t vkrndr::backend_t::image_count() const
{
    return swap_chain_->image_count();
}

uint32_t vkrndr::backend_t::frames_in_flight() const
{
    return render_settings_.frames_in_flight;
}

VkExtent2D vkrndr::backend_t::extent() const { return swap_chain_->extent(); }

vkrndr::image_t vkrndr::backend_t::swapchain_image()
{
    return {.image = swap_chain_->image(image_index_),
        .allocation = VK_NULL_HANDLE,
        .view = swap_chain_->image_view(image_index_),
        .format = swap_chain_->image_format(),
        .sample_count = VK_SAMPLE_COUNT_1_BIT,
        .mip_levels = 1,
        .extent = vkrndr::to_3d_extent(swap_chain_->extent())};
}

vkrndr::swapchain_acquire_t vkrndr::backend_t::begin_frame()
{
    if (window_->is_minimized())
    {
        return false;
    }

    if (swap_chain_refresh.load())
    {
        spdlog::info("Recreating swapchain");
        vkDeviceWaitIdle(device_.logical);
        swap_chain_->recreate();
        swap_chain_refresh.store(false);
        return {swap_chain_->extent()};
    }

    if (!swap_chain_->acquire_next_image(frame_data_.index(), image_index_))
    {
        return false;
    }

    frame_data_->present_command_pool->reset();
    frame_data_->transfer_transient_command_pool->reset();

    return true;
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

        frame_data_->present_command_pool->allocate_command_buffers(true,
            1,
            cppext::as_span(frame_data_->present_command_buffers.back()));
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
        return {*frame_data_->transfer_queue,
            *frame_data_->transfer_transient_command_pool};
    }

    return {*frame_data_->present_queue,
        *frame_data_->present_transient_command_pool};
}

void vkrndr::backend_t::draw()
{
    std::span const buffers{frame_data_->present_command_buffers.data(),
        frame_data_->used_present_command_buffers};

    for (VkCommandBuffer const buffer : buffers)
    {
        check_result(vkEndCommandBuffer(buffer));
    }

    swap_chain_->submit_command_buffers(buffers,
        frame_data_.index(),
        image_index_);
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
