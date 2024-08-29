#include <vkrndr_backend.hpp>

#include <vkrndr_buffer.hpp>
#include <vkrndr_commands.hpp>
#include <vkrndr_context.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_global_data.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_memory.hpp>
#include <vkrndr_scene.hpp>
#include <vkrndr_swap_chain.hpp>
#include <vkrndr_utility.hpp>
#include <vkrndr_window.hpp>

#include <cppext_cycled_buffer.hpp>
#include <cppext_numeric.hpp>

#include <stb_image.h>

#include <array>
#include <cstring>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

// IWYU pragma: no_include <functional>
// IWYU pragma: no_include <unordered_map>

namespace
{
    VkDescriptorPool create_descriptor_pool(vkrndr::device_t const& device)
    {
        constexpr auto count{
            vkrndr::count_cast(vkrndr::swap_chain_t::max_frames_in_flight)};

        VkDescriptorPoolSize uniform_buffer_pool_size{};
        uniform_buffer_pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uniform_buffer_pool_size.descriptorCount = 3 * count;

        VkDescriptorPoolSize storage_buffer_pool_size{};
        storage_buffer_pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        storage_buffer_pool_size.descriptorCount = 6 * count;

        VkDescriptorPoolSize texture_sampler_pool_size{};
        texture_sampler_pool_size.type =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        texture_sampler_pool_size.descriptorCount = 2 * count;

        std::array pool_sizes{uniform_buffer_pool_size,
            storage_buffer_pool_size,
            texture_sampler_pool_size};

        VkDescriptorPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.poolSizeCount = vkrndr::count_cast(pool_sizes.size());
        pool_info.pPoolSizes = pool_sizes.data();
        pool_info.maxSets = 4 * count + count;

        VkDescriptorPool rv{};
        vkrndr::check_result(
            vkCreateDescriptorPool(device.logical, &pool_info, nullptr, &rv));

        return rv;
    }
} // namespace

vkrndr::backend_t::backend_t(window_t* const window,
    render_settings_t const& settings,
    bool const debug)
    : render_settings_{settings}
    , window_{window}
    , context_{vkrndr::create_context(window, debug)}
    , device_{vkrndr::create_device(context_)}
    , swap_chain_{std::make_unique<swap_chain_t>(window_,
          &context_,
          &device_,
          &render_settings_)}
    , frame_data_{swap_chain_t::max_frames_in_flight,
          swap_chain_t::max_frames_in_flight}
    , descriptor_pool_{create_descriptor_pool(device_)}
{
    for (frame_data_t& fd : frame_data_.as_span())
    {
        fd.present_queue = device_.present_queue;
        fd.present_command_pool = std::make_unique<command_pool_t>(device_,
            fd.present_queue->queue_family());

        if (device_.transfer_queue)
        {
            fd.transfer_queue = device_.transfer_queue;
            fd.transfer_command_pool = std::make_unique<command_pool_t>(device_,
                fd.transfer_queue->queue_family());
        }
    };
}

vkrndr::backend_t::~backend_t()
{
    for (frame_data_t& fd : frame_data_.as_span())
    {
        fd.present_command_pool.reset();
        fd.transfer_command_pool.reset();
    };

    vkDestroyDescriptorPool(device_.logical, descriptor_pool_, nullptr);

    swap_chain_.reset();

    destroy(&device_);
    destroy(&context_);
}

VkFormat vkrndr::backend_t::image_format() const
{
    return swap_chain_->image_format();
}

uint32_t vkrndr::backend_t::image_count() const
{
    return swap_chain_->image_count();
}

VkExtent2D vkrndr::backend_t::extent() const { return swap_chain_->extent(); }

vkrndr::swapchain_acquire_t vkrndr::backend_t::begin_frame()
{
    if (window_->is_minimized())
    {
        return {};
    }

    if (swap_chain_refresh.load())
    {
        vkDeviceWaitIdle(device_.logical);
        swap_chain_->recreate();
        swap_chain_refresh.store(false);
        return {extent()};
    }

    if (!swap_chain_->acquire_next_image(frame_data_.index(), image_index_))
    {
        return {};
    }

    frame_data_->present_command_pool->reset();
    if (frame_data_->transfer_queue)
    {
        frame_data_->transfer_command_pool->reset();
    }

    VkCommandBuffer primary_buffer{request_command_buffer(false)};

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    check_result(vkBeginCommandBuffer(primary_buffer, &begin_info));

    return image_t{.image = swap_chain_->image(image_index_),
        .allocation = VK_NULL_HANDLE,
        .view = swap_chain_->image_view(image_index_),
        .format = swap_chain_->image_format(),
        .sample_count = VK_SAMPLE_COUNT_1_BIT,
        .mip_levels = 0,
        .extent = extent()};
}

void vkrndr::backend_t::end_frame()
{
    frame_data_.cycle(
        [](frame_data_t& fd, frame_data_t const&)
        {
            fd.used_present_command_buffers = 0;
            fd.used_transfer_command_buffers = 0;
        });
}

VkCommandBuffer vkrndr::backend_t::request_command_buffer(
    bool const transfer_only)
{
    if (transfer_only &&
        frame_data_->present_queue != frame_data_->transfer_queue)
    {
        if (frame_data_->used_transfer_command_buffers ==
            frame_data_->transfer_command_buffers.size())
        {
            frame_data_->transfer_command_buffers.resize(
                frame_data_->transfer_command_buffers.size() + 1);

            frame_data_->transfer_command_pool->allocate_command_buffers(true,
                1,
                std::span{&frame_data_->transfer_command_buffers.back(), 1});
        }

        VkCommandBuffer rv{frame_data_->transfer_command_buffers
                               [frame_data_->used_transfer_command_buffers++]};
        return rv;
    }

    if (frame_data_->used_present_command_buffers ==
        frame_data_->present_command_buffers.size())
    {
        frame_data_->present_command_buffers.resize(
            frame_data_->present_command_buffers.size() + 1);

        frame_data_->present_command_pool->allocate_command_buffers(true,
            1,
            std::span{&frame_data_->present_command_buffers.back(), 1});
    }

    VkCommandBuffer rv{frame_data_->present_command_buffers
                           [frame_data_->used_present_command_buffers++]};
    return rv;
}

void vkrndr::backend_t::draw(scene_t& scene, image_t const& target_image)
{
    VkCommandBuffer command_buffer{
        frame_data_->present_command_buffers
            [frame_data_->used_present_command_buffers - 1]};

    scene.draw(target_image, command_buffer, extent());

    check_result(vkEndCommandBuffer(command_buffer));

    swap_chain_->submit_command_buffers(
        std::span{frame_data_->present_command_buffers.data(),
            frame_data_->used_present_command_buffers},
        frame_data_.index(),
        image_index_);
}

vkrndr::image_t vkrndr::backend_t::load_texture(
    std::filesystem::path const& texture_path,
    VkFormat const format)
{
    int width; // NOLINT
    int height; // NOLINT
    int channels; // NOLINT
    stbi_uc* const pixels{stbi_load(texture_path.string().c_str(),
        &width,
        &height,
        &channels,
        STBI_rgb_alpha)};

    auto const image_size{static_cast<VkDeviceSize>(width * height * 4)};

    if (!pixels)
    {
        throw std::runtime_error{"failed to load texture image!"};
    }

    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)
    auto const image_bytes{
        std::span{reinterpret_cast<std::byte const*>(pixels), image_size}};
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
    image_t rv{transfer_image(image_bytes,
        {cppext::narrow<uint32_t>(width), cppext::narrow<uint32_t>(height)},
        format,
        max_mip_levels(width, height))};

    stbi_image_free(pixels);

    return rv;
}

vkrndr::image_t vkrndr::backend_t::transfer_image(
    std::span<std::byte const> image_data,
    VkExtent2D const extent,
    VkFormat const format,
    uint32_t const mip_levels)
{
    buffer_t staging_buffer{create_buffer(device_,
        image_data.size(),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};

    mapped_memory_t staging_map{map_memory(device_, staging_buffer)};
    memcpy(staging_map.mapped_memory, image_data.data(), image_data.size());
    unmap_memory(device_, &staging_map);

    image_t rv{
        transfer_buffer_to_image(staging_buffer, extent, format, mip_levels)};

    destroy(&device_, &staging_buffer);

    return rv;
}

vkrndr::image_t vkrndr::backend_t::transfer_buffer_to_image(
    vkrndr::buffer_t const& source,
    VkExtent2D const extent,
    VkFormat const format,
    uint32_t const mip_levels)
{
    image_t image{create_image_and_view(device_,
        extent,
        mip_levels,
        VK_SAMPLE_COUNT_1_BIT,
        format,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT)};

    execution_port_t* const queue{frame_data_->present_queue};

    VkCommandBuffer present_queue_buffer; // NOLINT
    begin_single_time_commands(*frame_data_->present_command_pool,
        1,
        std::span{&present_queue_buffer, 1});

    wait_for_transfer_write(image.image, present_queue_buffer, mip_levels);
    copy_buffer_to_image(present_queue_buffer,
        source.buffer,
        image.image,
        extent);
    if (mip_levels == 1)
    {
        wait_for_transfer_write_completed(image.image,
            present_queue_buffer,
            mip_levels);
    }
    else
    {
        generate_mipmaps(device_,
            image.image,
            present_queue_buffer,
            format,
            extent,
            mip_levels);
    }

    end_single_time_commands(*frame_data_->present_command_pool,
        *queue,
        std::span{&present_queue_buffer, 1});

    return image;
}

void vkrndr::backend_t::transfer_buffer(buffer_t const& source,
    buffer_t const& target)
{
    execution_port_t* const transfer_queue{frame_data_->transfer_queue};

    VkCommandBuffer command_buffer; // NOLINT
    begin_single_time_commands(*frame_data_->transfer_command_pool,
        1,
        std::span{&command_buffer, 1});

    copy_buffer_to_buffer(command_buffer,
        source.buffer,
        source.size,
        target.buffer);

    end_single_time_commands(*frame_data_->transfer_command_pool,
        *transfer_queue,
        std::span{&command_buffer, 1});
}
