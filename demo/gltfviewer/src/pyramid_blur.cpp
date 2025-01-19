#include <pyramid_blur.hpp>

#include <config.hpp>

#include <cppext_container.hpp>
#include <cppext_cycled_buffer.hpp>
#include <cppext_numeric.hpp>

#include <vkglsl_shader_set.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_debug_utils.hpp>
#include <vkrndr_descriptors.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_pipeline.hpp>
#include <vkrndr_shader_module.hpp>
#include <vkrndr_utility.hpp>

#include <boost/scope/defer.hpp>

#include <glm/vec2.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <ranges>
#include <span>
#include <vector>

// IWYU pragma: no_include <expected>
// IWYU pragma: no_include <filesystem>
// IWYU pragma: no_include <iterator>
// IWYU pragma: no_include <memory>
// IWYU pragma: no_include <system_error>

namespace
{
    struct [[nodiscard]] push_constants_t final
    {
        glm::uvec2 source_resolution;
        uint32_t source_mip;
    };

    [[nodiscard]] VkSampler create_sampler(vkrndr::device_t const& device)
    {
        VkPhysicalDeviceProperties properties; // NOLINT
        vkGetPhysicalDeviceProperties(device.physical, &properties);

        VkSamplerCreateInfo sampler_info{};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = VK_FILTER_LINEAR;
        sampler_info.minFilter = VK_FILTER_LINEAR;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.anisotropyEnable = VK_TRUE;
        sampler_info.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
        sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        sampler_info.unnormalizedCoordinates = VK_FALSE;
        sampler_info.compareEnable = VK_FALSE;
        sampler_info.compareOp = VK_COMPARE_OP_NEVER;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler_info.mipLodBias = 0.0f;
        sampler_info.minLod = 0.0f;
        sampler_info.maxLod = 1.0f;

        VkSampler rv; // NOLINT
        vkrndr::check_result(
            vkCreateSampler(device.logical, &sampler_info, nullptr, &rv));

        return rv;
    }

    void transition_mip(VkImage const image,
        VkCommandBuffer const command_buffer,
        VkImageLayout const old_layout,
        VkPipelineStageFlags2 const src_stage_mask,
        VkAccessFlags2 const src_access_mask,
        VkImageLayout const new_layout,
        VkPipelineStageFlags2 const dst_stage_mask,
        VkAccessFlags2 const dst_access_mask,
        uint32_t const mip_level)
    {
        VkImageMemoryBarrier2 barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barrier.oldLayout = old_layout;
        barrier.srcStageMask = src_stage_mask;
        barrier.srcAccessMask = src_access_mask;
        barrier.newLayout = new_layout;
        barrier.dstStageMask = dst_stage_mask;
        barrier.dstAccessMask = dst_access_mask;
        barrier.image = image;
        barrier.subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = mip_level,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        };

        VkDependencyInfo dependency{};
        dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependency.imageMemoryBarrierCount = 1;
        dependency.pImageMemoryBarriers = &barrier;

        vkCmdPipelineBarrier2(command_buffer, &dependency);
    }

    void update_descriptor_set(vkrndr::device_t const& device,
        VkDescriptorSet const descriptor_set,
        VkSampler const sampler,
        std::vector<VkImageView> const& mip_views)
    {
        std::vector<VkDescriptorImageInfo> sampler_info;
        sampler_info.reserve(mip_views.size());

        std::vector<VkDescriptorImageInfo> storage_info;
        storage_info.reserve(mip_views.size());

        for (VkImageView view : mip_views)
        {
            sampler_info.emplace_back(sampler, view, VK_IMAGE_LAYOUT_GENERAL);
            storage_info.emplace_back(VK_NULL_HANDLE,
                view,
                VK_IMAGE_LAYOUT_GENERAL);
        }

        VkWriteDescriptorSet sampler_write{};
        sampler_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        sampler_write.dstSet = descriptor_set;
        sampler_write.dstBinding = 0;
        sampler_write.dstArrayElement = 0;
        sampler_write.descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        sampler_write.descriptorCount =
            cppext::narrow<uint32_t>(sampler_info.size());
        sampler_write.pImageInfo = sampler_info.data();

        VkWriteDescriptorSet storage_write{};
        storage_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        storage_write.dstSet = descriptor_set;
        storage_write.dstBinding = 1;
        storage_write.dstArrayElement = 0;
        storage_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        storage_write.descriptorCount =
            cppext::narrow<uint32_t>(storage_info.size());
        storage_write.pImageInfo = storage_info.data();

        std::array const descriptor_writes{sampler_write, storage_write};

        vkUpdateDescriptorSets(device.logical,
            vkrndr::count_cast(descriptor_writes.size()),
            descriptor_writes.data(),
            0,
            nullptr);
    }
} // namespace

gltfviewer::pyramid_blur_t::pyramid_blur_t(vkrndr::backend_t& backend)
    : backend_{&backend}
    , bilinear_sampler_{create_sampler(backend_->device())}
    , frame_data_{backend_->frames_in_flight(), backend_->frames_in_flight()}
{
}

gltfviewer::pyramid_blur_t::~pyramid_blur_t()
{
    for (frame_data_t& data : cppext::as_span(frame_data_))
    {
        vkrndr::free_descriptor_sets(backend_->device(),
            backend_->descriptor_pool(),
            cppext::as_span(data.descriptor_));
    }

    destroy(&backend_->device(), &upsample_pipeline_);

    destroy(&backend_->device(), &downsample_pipeline_);

    vkDestroyDescriptorSetLayout(backend_->device().logical,
        descriptor_layout_,
        nullptr);

    for (VkImageView view : mip_views_)
    {
        vkDestroyImageView(backend_->device().logical, view, nullptr);
    }

    destroy(&backend_->device(), &pyramid_image_);

    vkDestroySampler(backend_->device().logical, bilinear_sampler_, nullptr);
}

vkrndr::image_t gltfviewer::pyramid_blur_t::source_image() const
{
    auto rv{pyramid_image_};
    rv.view = mip_views_.front();
    return rv;
}

void gltfviewer::pyramid_blur_t::draw(uint32_t const levels,
    VkCommandBuffer command_buffer)
{
    frame_data_.cycle();

    [[maybe_unused]] vkrndr::command_buffer_scope_t const cb_scope{
        command_buffer,
        "Pyramid blur"};

    auto const l{
        std::clamp(levels, uint32_t{1}, pyramid_image_.mip_levels - 1)};
    downsample_pass(l, command_buffer);
    upsample_pass(l, command_buffer);
}

void gltfviewer::pyramid_blur_t::downsample_pass(uint32_t const levels,
    VkCommandBuffer command_buffer)
{
    vkrndr::bind_pipeline(command_buffer,
        downsample_pipeline_,
        0,
        cppext::as_span(frame_data_->descriptor_));

    for (uint32_t mip{}; mip != levels; ++mip)
    {
        transition_mip(pyramid_image_.image,
            command_buffer,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            mip);

        transition_mip(pyramid_image_.image,
            command_buffer,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            mip + 1);

        push_constants_t pc{
            .source_resolution =
                glm::uvec2{mip_extents_[mip].width, mip_extents_[mip].height},
            .source_mip = mip};
        vkCmdPushConstants(command_buffer,
            *downsample_pipeline_.layout,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0,
            sizeof(pc),
            &pc);

        vkCmdDispatch(command_buffer,
            static_cast<uint32_t>(
                std::ceil(cppext::as_fp(mip_extents_[mip + 1].width) / 16.0f)),
            static_cast<uint32_t>(
                std::ceil(cppext::as_fp(mip_extents_[mip + 1].height) / 16.0f)),
            1);
    }
}

void gltfviewer::pyramid_blur_t::upsample_pass(uint32_t const levels,
    VkCommandBuffer command_buffer)
{
    vkrndr::bind_pipeline(command_buffer,
        upsample_pipeline_,
        0,
        cppext::as_span(frame_data_->descriptor_));

    for (uint32_t const mip :
        std::views::iota(uint32_t{0}, levels) | std::views::reverse)
    {
        transition_mip(pyramid_image_.image,
            command_buffer,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            mip + 1);

        transition_mip(pyramid_image_.image,
            command_buffer,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            mip);

        push_constants_t pc{
            .source_resolution =
                glm::uvec2{mip_extents_[mip].width, mip_extents_[mip].height},
            .source_mip = mip};
        vkCmdPushConstants(command_buffer,
            *upsample_pipeline_.layout,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0,
            sizeof(pc),
            &pc);

        vkCmdDispatch(command_buffer,
            static_cast<uint32_t>(
                std::ceil(cppext::as_fp(mip_extents_[mip].width) / 16.0f)),
            static_cast<uint32_t>(
                std::ceil(cppext::as_fp(mip_extents_[mip].height) / 16.0f)),
            1);
    }
}

void gltfviewer::pyramid_blur_t::resize(uint32_t const width,
    uint32_t const height)
{
    destroy(&backend_->device(), &pyramid_image_);
    pyramid_image_ = vkrndr::create_image(backend_->device(),
        vkrndr::image_2d_create_info_t{.format = VK_FORMAT_R16G16B16A16_SFLOAT,
            .extent = vkrndr::to_2d_extent(width, height),
            .mip_levels = vkrndr::max_mip_levels(width, height),
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .required_memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT});

    for (VkImageView view : mip_views_)
    {
        vkDestroyImageView(backend_->device().logical, view, nullptr);
    }

    mip_views_.resize(pyramid_image_.mip_levels);
    mip_extents_.resize(pyramid_image_.mip_levels);

    auto [mip_width, mip_height] = vkrndr::to_2d_extent(pyramid_image_.extent);
    for (uint32_t i{}; i != pyramid_image_.mip_levels; ++i)
    {
        mip_views_[i] = vkrndr::create_image_view(backend_->device(),
            pyramid_image_.image,
            pyramid_image_.format,
            VK_IMAGE_ASPECT_COLOR_BIT,
            1,
            i);

        mip_extents_[i] = {mip_width, mip_height};

        mip_width = std::max<uint32_t>(1, mip_width / 2);
        mip_height = std::max<uint32_t>(1, mip_height / 2);
    }

    object_name(backend_->device(), pyramid_image_, "Pyramid Image");

    create_downsample_resources();
    create_upsample_resources();
}

void gltfviewer::pyramid_blur_t::create_downsample_resources()
{
    vkglsl::shader_set_t shader_set{enable_shader_debug_symbols,
        enable_shader_optimization};

    auto shader{add_shader_module_from_path(shader_set,
        backend_->device(),
        VK_SHADER_STAGE_COMPUTE_BIT,
        "pyramid_downsample.comp")};
    assert(shader);
    [[maybe_unused]] boost::scope::defer_guard const destroy_shd{
        [this, shd = &shader.value()]() { destroy(&backend_->device(), shd); }};

    auto bindings{shader_set.descriptor_bindings(0)};
    assert(bindings);

    (*bindings)[0].descriptorCount = pyramid_image_.mip_levels;
    (*bindings)[1].descriptorCount = pyramid_image_.mip_levels;

    vkDestroyDescriptorSetLayout(backend_->device().logical,
        descriptor_layout_,
        nullptr);
    descriptor_layout_ =
        vkrndr::create_descriptor_set_layout(backend_->device(), *bindings);

    for (frame_data_t& data : cppext::as_span(frame_data_))
    {
        auto ds{cppext::as_span(data.descriptor_)};

        vkrndr::free_descriptor_sets(backend_->device(),
            backend_->descriptor_pool(),
            ds);

        vkrndr::create_descriptor_sets(backend_->device(),
            backend_->descriptor_pool(),
            cppext::as_span(descriptor_layout_),
            ds);

        update_descriptor_set(backend_->device(),
            data.descriptor_,
            bilinear_sampler_,
            mip_views_);
    }

    destroy(&backend_->device(), &downsample_pipeline_);
    downsample_pipeline_ =
        vkrndr::compute_pipeline_builder_t{backend_->device(),
            vkrndr::pipeline_layout_builder_t{backend_->device()}
                .add_push_constants<push_constants_t>(
                    VK_SHADER_STAGE_COMPUTE_BIT)
                .add_descriptor_set_layout(descriptor_layout_)
                .build()}
            .with_shader(as_pipeline_shader(*shader))
            .build();
}

void gltfviewer::pyramid_blur_t::create_upsample_resources()
{
    vkglsl::shader_set_t shader_set{enable_shader_debug_symbols,
        enable_shader_optimization};

    auto shader{add_shader_module_from_path(shader_set,
        backend_->device(),
        VK_SHADER_STAGE_COMPUTE_BIT,
        "pyramid_upsample.comp")};
    assert(shader);
    [[maybe_unused]] boost::scope::defer_guard const destroy_shd{
        [this, shd = &shader.value()]() { destroy(&backend_->device(), shd); }};

    destroy(&backend_->device(), &upsample_pipeline_);
    upsample_pipeline_ = vkrndr::compute_pipeline_builder_t{backend_->device(),
        downsample_pipeline_.layout}
                             .with_shader(as_pipeline_shader(*shader))
                             .build();
}
