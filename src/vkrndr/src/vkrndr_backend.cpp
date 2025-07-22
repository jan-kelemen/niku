#include <vkrndr_backend.hpp>

#include <vkrndr_buffer.hpp>
#include <vkrndr_commands.hpp>
#include <vkrndr_descriptors.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_execution_port.hpp>
#include <vkrndr_features.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_instance.hpp>
#include <vkrndr_library_handle.hpp>
#include <vkrndr_memory.hpp>
#include <vkrndr_transient_operation.hpp>
#include <vkrndr_utility.hpp>

#include <cppext_container.hpp>
#include <cppext_cycled_buffer.hpp>
#include <cppext_numeric.hpp>

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
} // namespace

vkrndr::backend_t::backend_t(rendering_context_t rendering_context,
    uint32_t frames_in_flight)
    : context_{std::move(rendering_context)}
{
    auto const execution_port{
        std::ranges::find_if(context_.device->execution_ports,
            [](execution_port_t const& port)
            { return port.has_graphics() && port.has_transfer(); })};
    if (execution_port == std::cend(context_.device->execution_ports))
    {
        throw std::runtime_error{"no suitable execution port found"};
    }

    frame_data_ = cppext::cycled_buffer_t<frame_data_t>{frames_in_flight,
        frames_in_flight};
    descriptor_pool_ = ::create_descriptor_pool(*context_.device);

    for (frame_data_t& fd : cppext::as_span(frame_data_))
    {
        fd.present_queue = &(*execution_port);
        fd.present_command_pool = create_command_pool(*context_.device,
            fd.present_queue->queue_family())
                                      .value();
        fd.present_transient_command_pool =
            create_command_pool(*context_.device,
                fd.present_queue->queue_family(),
                VK_COMMAND_POOL_CREATE_TRANSIENT_BIT)
                .value();

        fd.transfer_queue = &(*execution_port);
        fd.transfer_transient_command_pool =
            create_command_pool(*context_.device,
                fd.transfer_queue->queue_family())
                .value();
    };
}

vkrndr::backend_t::~backend_t()
{
    for (frame_data_t const& fd : cppext::as_span(frame_data_))
    {
        destroy_command_pool(*context_.device, fd.present_command_pool);
        destroy_command_pool(*context_.device,
            fd.present_transient_command_pool);
        destroy_command_pool(*context_.device,
            fd.transfer_transient_command_pool);
    };

    destroy_descriptor_pool(*context_.device, descriptor_pool_);
}

vkrndr::instance_t& vkrndr::backend_t::instance() noexcept
{
    return *context_.instance;
}

vkrndr::instance_t const& vkrndr::backend_t::instance() const noexcept
{
    return *context_.instance;
}

vkrndr::device_t& vkrndr::backend_t::device() noexcept
{
    return *context_.device;
}

vkrndr::device_t const& vkrndr::backend_t::device() const noexcept
{
    return *context_.device;
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
    check_result(reset_command_pool(*context_.device,
        frame_data_->present_command_pool));
    check_result(reset_command_pool(*context_.device,
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

        check_result(allocate_command_buffers(*context_.device,
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
        return {*context_.device,
            *frame_data_->transfer_queue,
            frame_data_->transfer_transient_command_pool};
    }

    return {*context_.device,
        *frame_data_->present_queue,
        frame_data_->present_transient_command_pool};
}

vkrndr::image_t vkrndr::backend_t::transfer_image(
    std::span<std::byte const> const& image_data,
    VkExtent2D const extent,
    VkFormat const format,
    uint32_t const mip_levels)
{
    buffer_t staging_buffer{
        create_staging_buffer(*context_.device, image_data.size())};
    boost::scope::defer_guard const rollback{[this, staging_buffer]() mutable
        { destroy(context_.device.get(), &staging_buffer); }};

    {
        mapped_memory_t staging_map{
            map_memory(*context_.device, staging_buffer)};
        memcpy(staging_map.mapped_memory, image_data.data(), image_data.size());
        unmap_memory(*context_.device, &staging_map);
    }

    return transfer_buffer_to_image(staging_buffer, extent, format, mip_levels);
}

vkrndr::image_t vkrndr::backend_t::transfer_buffer_to_image(
    vkrndr::buffer_t const& source,
    VkExtent2D const extent,
    VkFormat const format,
    uint32_t const mip_levels)
{
    image_t image{create_image_and_view(*context_.device,
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
        [this, &image]() mutable { destroy(context_.device.get(), &image); }};

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
            generate_mipmaps(*context_.device,
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
