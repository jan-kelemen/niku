#include <vulkan_renderer.hpp>

#include <font_manager.hpp>
#include <global_data.hpp>
#include <gltf_manager.hpp>
#include <imgui_render_layer.hpp>
#include <vkrndr_scene.hpp>
#include <vulkan_buffer.hpp>
#include <vulkan_commands.hpp>
#include <vulkan_context.hpp>
#include <vulkan_device.hpp>
#include <vulkan_font.hpp>
#include <vulkan_image.hpp>
#include <vulkan_memory.hpp>
#include <vulkan_queue.hpp>
#include <vulkan_swap_chain.hpp>
#include <vulkan_utility.hpp>
#include <vulkan_window.hpp>

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
    VkDescriptorPool create_descriptor_pool(vkrndr::vulkan_device const& device)
    {
        constexpr auto count{vkrndr::count_cast(
            vkrndr::vulkan_swap_chain::max_frames_in_flight)};

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

vkrndr::vulkan_renderer::vulkan_renderer(vulkan_window* const window,
    render_settings const& settings,
    bool const debug)
    : render_settings_{settings}
    , window_{window}
    , context_{vkrndr::create_context(window, debug)}
    , device_{vkrndr::create_device(context_)}
    , swap_chain_{std::make_unique<vulkan_swap_chain>(window_,
          &context_,
          &device_,
          &render_settings_)}
    , frame_data_{vulkan_swap_chain::max_frames_in_flight,
          vulkan_swap_chain::max_frames_in_flight}
    , descriptor_pool_{create_descriptor_pool(device_)}
    , imgui_layer_enabled_{debug}
    , font_manager_{std::make_unique<font_manager>()}
    , gltf_manager_{std::make_unique<gltf_manager>(this)}
{
    if (imgui_layer_enabled_)
    {
        imgui_layer(true);
    }

    for (frame_data& fd : frame_data_.as_span())
    {
        fd.present_queue = device_.present_queue;
        fd.present_command_pool =
            create_command_pool(device_, fd.present_queue->family);

        if (device_.present_queue == device_.transfer_queue)
        {
            fd.transfer_queue = fd.present_queue;
            fd.transfer_command_pool = fd.present_command_pool;
        }
        else
        {
            fd.transfer_queue = device_.transfer_queue;
            fd.transfer_command_pool =
                create_command_pool(device_, fd.transfer_queue->family);
        }
    }
}

vkrndr::vulkan_renderer::~vulkan_renderer()
{
    imgui_layer_.reset();

    for (frame_data const& fd : frame_data_.as_span())
    {
        if (fd.present_queue != fd.transfer_queue)
        {
            vkDestroyCommandPool(device_.logical,
                fd.transfer_command_pool,
                nullptr);
        }
        vkDestroyCommandPool(device_.logical, fd.present_command_pool, nullptr);
    }

    vkDestroyDescriptorPool(device_.logical, descriptor_pool_, nullptr);

    swap_chain_.reset();

    destroy(&device_);
    destroy(&context_);
}

VkFormat vkrndr::vulkan_renderer::image_format() const
{
    return swap_chain_->image_format();
}

uint32_t vkrndr::vulkan_renderer::image_count() const
{
    return swap_chain_->image_count();
}

VkExtent2D vkrndr::vulkan_renderer::extent() const
{
    return swap_chain_->extent();
}

bool vkrndr::vulkan_renderer::imgui_layer() const
{
    return imgui_layer_enabled_;
}

void vkrndr::vulkan_renderer::imgui_layer(bool const state)
{
    if (state && !imgui_layer_)
    {
        imgui_layer_ = std::make_unique<imgui_render_layer>(window_,
            &context_,
            &device_,
            swap_chain_.get());
    }
    imgui_layer_enabled_ = state;
}

bool vkrndr::vulkan_renderer::begin_frame(scene* const scene)
{
    if (swap_chain_refresh.load())
    {
        if (window_->is_minimized())
        {
            return false;
        }

        vkDeviceWaitIdle(device_.logical);
        swap_chain_->recreate();
        scene->resize(extent());
        swap_chain_refresh.store(false);
        return false;
    }

    if (!swap_chain_->acquire_next_image(frame_data_.index(), image_index_))
    {
        return false;
    }

    VkCommandBuffer primary_buffer{request_command_buffer(false)};

    check_result(vkResetCommandBuffer(primary_buffer, 0));

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    check_result(vkBeginCommandBuffer(primary_buffer, &begin_info));

    if (imgui_layer_enabled_)
    {
        imgui_layer_->begin_frame();
    }

    return true;
}

void vkrndr::vulkan_renderer::end_frame()
{
    if (imgui_layer_enabled_)
    {
        imgui_layer_->end_frame();
    }

    frame_data_.cycle(
        [](frame_data& fd, frame_data const&)
        {
            fd.used_present_command_buffers_ = 0;
            fd.used_transfer_command_buffers = 0;
        });
}

void vkrndr::vulkan_renderer::draw(scene* const scene)
{
    VkCommandBuffer command_buffer{
        frame_data_->present_command_buffers
            [frame_data_->used_present_command_buffers_ - 1]};

    vulkan_image target_image{.image = swap_chain_->image(image_index_),
        .allocation = VK_NULL_HANDLE,
        .view = swap_chain_->image_view(image_index_),
        .format = swap_chain_->image_format(),
        .sample_count = VK_SAMPLE_COUNT_1_BIT,
        .mip_levels = 0,
        .extent = extent()};

    scene->draw(target_image, command_buffer, extent());

    if (imgui_layer_enabled_)
    {
        scene->draw_imgui();
        VkCommandBuffer imgui_command_buffer{request_command_buffer(false)};
        imgui_layer_->draw(imgui_command_buffer,
            swap_chain_->image(image_index_),
            swap_chain_->image_view(image_index_),
            extent());
    }

    check_result(vkEndCommandBuffer(command_buffer));

    swap_chain_->submit_command_buffers(
        std::span{frame_data_->present_command_buffers.data(),
            frame_data_->used_present_command_buffers_},
        frame_data_.index(),
        image_index_);
}

vkrndr::vulkan_image vkrndr::vulkan_renderer::load_texture(
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
    vulkan_image rv{transfer_image(image_bytes,
        {cppext::narrow<uint32_t>(width), cppext::narrow<uint32_t>(height)},
        format,
        max_mip_levels(width, height))};

    stbi_image_free(pixels);

    return rv;
}

vkrndr::vulkan_image vkrndr::vulkan_renderer::transfer_image(
    std::span<std::byte const> image_data,
    VkExtent2D const extent,
    VkFormat const format,
    uint32_t const mip_levels)
{
    vulkan_buffer staging_buffer{create_buffer(device_,
        image_data.size(),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};

    mapped_memory staging_map{map_memory(device_, staging_buffer)};
    memcpy(staging_map.mapped_memory, image_data.data(), image_data.size());
    unmap_memory(device_, &staging_map);

    vulkan_image rv{
        transfer_buffer_to_image(staging_buffer, extent, format, mip_levels)};

    destroy(&device_, &staging_buffer);

    return rv;
}

vkrndr::vulkan_image vkrndr::vulkan_renderer::transfer_buffer_to_image(
    vkrndr::vulkan_buffer const& source,
    VkExtent2D const extent,
    VkFormat const format,
    uint32_t const mip_levels)
{
    vulkan_image image{create_image_and_view(device_,
        extent,
        mip_levels,
        VK_SAMPLE_COUNT_1_BIT,
        format,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT)};

    vulkan_queue* const queue{frame_data_->present_queue};

    VkCommandBuffer present_queue_buffer; // NOLINT
    begin_single_time_commands(device_,
        frame_data_->present_command_pool,
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

    end_single_time_commands(device_,
        queue->queue,
        std::span{&present_queue_buffer, 1},
        frame_data_->present_command_pool);

    return image;
}

void vkrndr::vulkan_renderer::transfer_buffer(vulkan_buffer const& source,
    vulkan_buffer const& target)
{
    vulkan_queue* const transfer_queue{frame_data_->transfer_queue};

    VkCommandBuffer command_buffer; // NOLINT
    begin_single_time_commands(device_,
        frame_data_->transfer_command_pool,
        1,
        std::span{&command_buffer, 1});

    copy_buffer_to_buffer(command_buffer,
        source.buffer,
        source.size,
        target.buffer);

    end_single_time_commands(device_,
        transfer_queue->queue,
        std::span{&command_buffer, 1},
        frame_data_->transfer_command_pool);
}

vkrndr::vulkan_font vkrndr::vulkan_renderer::load_font(
    std::filesystem::path const& font_path,
    uint32_t const font_size)
{
    font_bitmap font_bitmap{
        font_manager_->load_font_bitmap(font_path, font_size)};

    auto const image_size{static_cast<VkDeviceSize>(
        font_bitmap.bitmap_width * font_bitmap.bitmap_height)};

    vulkan_buffer staging_buffer{create_buffer(device_,
        image_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};

    mapped_memory staging_map{map_memory(device_, staging_buffer)};
    memcpy(staging_map.mapped_memory,
        font_bitmap.bitmap_data.data(),
        static_cast<size_t>(image_size));
    unmap_memory(device_, &staging_map);

    VkExtent2D const bitmap_extent{font_bitmap.bitmap_width,
        font_bitmap.bitmap_height};
    vulkan_image const texture{create_image_and_view(device_,
        bitmap_extent,
        1,
        VK_SAMPLE_COUNT_1_BIT,
        VK_FORMAT_R8_UNORM,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT)};

    vulkan_queue* const queue{device_.present_queue};

    VkCommandBuffer present_queue_buffer; // NOLINT
    begin_single_time_commands(device_,
        frame_data_->present_command_pool,
        1,
        std::span{&present_queue_buffer, 1});

    wait_for_transfer_write(texture.image, present_queue_buffer, 1);

    copy_buffer_to_image(present_queue_buffer,
        staging_buffer.buffer,
        texture.image,
        bitmap_extent);

    wait_for_transfer_write_completed(texture.image, present_queue_buffer, 1);

    end_single_time_commands(device_,
        queue->queue,
        std::span{&present_queue_buffer, 1},
        frame_data_->present_command_pool);

    destroy(&device_, &staging_buffer);

    return {std::move(font_bitmap.bitmaps),
        font_bitmap.bitmap_width,
        font_bitmap.bitmap_height,
        texture};
}

std::unique_ptr<vkrndr::gltf_model> vkrndr::vulkan_renderer::load_model(
    std::filesystem::path const& model_path)
{
    return gltf_manager_->load(model_path);
}

VkCommandBuffer vkrndr::vulkan_renderer::request_command_buffer(
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

            create_command_buffers(device_,
                frame_data_->transfer_command_pool,
                1,
                VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                std::span{&frame_data_->transfer_command_buffers.back(), 1});
        }

        VkCommandBuffer rv{frame_data_->transfer_command_buffers
                               [frame_data_->used_transfer_command_buffers++]};
        check_result(vkResetCommandBuffer(rv, 0));
        return rv;
    }

    if (frame_data_->used_present_command_buffers_ ==
        frame_data_->present_command_buffers.size())
    {
        frame_data_->present_command_buffers.resize(
            frame_data_->present_command_buffers.size() + 1);

        create_command_buffers(device_,
            frame_data_->present_command_pool,
            1,
            VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            std::span{&frame_data_->present_command_buffers.back(), 1});
    }

    VkCommandBuffer rv{frame_data_->present_command_buffers
                           [frame_data_->used_present_command_buffers_++]};
    check_result(vkResetCommandBuffer(rv, 0));
    return rv;
}
