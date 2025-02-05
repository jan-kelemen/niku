#include <postprocess_shader.hpp>

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
#include <vkrndr_synchronization.hpp>
#include <vkrndr_utility.hpp>

#include <boost/scope/defer.hpp>

#include <vma_impl.hpp>

#include <volk.h>

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <span>

// IWYU pragma: no_include <expected>
// IWYU pragma: no_include <filesystem>
// IWYU pragma: no_include <memory>
// IWYU pragma: no_include <optional>
// IWYU pragma: no_include <vector>
// IWYU pragma: no_include <string_view>
// IWYU pragma: no_include <system_error>
// IWYU pragma: no_forward_declare VkDescriptorSet_T

namespace
{
    void update_descriptor_set(vkrndr::device_t const& device,
        VkDescriptorSet const descriptor_set,
        VkDescriptorImageInfo const combined_sampler_info,
        VkDescriptorImageInfo const target_image_info)
    {
        VkWriteDescriptorSet combined_sampler_uniform_write{};
        combined_sampler_uniform_write.sType =
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        combined_sampler_uniform_write.dstSet = descriptor_set;
        combined_sampler_uniform_write.dstBinding = 0;
        combined_sampler_uniform_write.dstArrayElement = 0;
        combined_sampler_uniform_write.descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        combined_sampler_uniform_write.descriptorCount = 1;
        combined_sampler_uniform_write.pImageInfo = &combined_sampler_info;

        VkWriteDescriptorSet target_image_uniform_write{};
        target_image_uniform_write.sType =
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        target_image_uniform_write.dstSet = descriptor_set;
        target_image_uniform_write.dstBinding = 1;
        target_image_uniform_write.dstArrayElement = 0;
        target_image_uniform_write.descriptorType =
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        target_image_uniform_write.descriptorCount = 1;
        target_image_uniform_write.pImageInfo = &target_image_info;

        std::array const descriptor_writes{combined_sampler_uniform_write,
            target_image_uniform_write};

        vkUpdateDescriptorSets(device.logical,
            vkrndr::count_cast(descriptor_writes.size()),
            descriptor_writes.data(),
            0,
            nullptr);
    }

    [[nodiscard]] vkrndr::image_t create_intermediate_image(
        vkrndr::backend_t const& backend,
        VkExtent2D const extent)
    {
        return vkrndr::create_image_and_view(backend.device(),
            vkrndr::image_2d_create_info_t{
                .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                .extent = extent,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage =
                    VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                .allocation_flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                .required_memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
            VK_IMAGE_ASPECT_COLOR_BIT);
    }

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

} // namespace

galileo::postprocess_shader_t::postprocess_shader_t(vkrndr::backend_t& backend)
    : backend_{&backend}
    , sampler_{create_sampler(backend_->device())}
    , frame_data_{backend_->frames_in_flight(), backend_->frames_in_flight()}
{
    {
        vkglsl::shader_set_t shader_set{enable_shader_debug_symbols,
            enable_shader_optimization};

        auto shader{add_shader_module_from_path(shader_set,
            backend_->device(),
            VK_SHADER_STAGE_COMPUTE_BIT,
            "tone_mapping.comp")};
        assert(shader);
        [[maybe_unused]] boost::scope::defer_guard const destroy_shd{
            [this, shd = &shader.value()]()
            { destroy(&backend_->device(), shd); }};

        if (auto const layout{
                descriptor_set_layout(shader_set, backend_->device(), 0)})
        {
            tone_mapping_descriptor_set_layout_ = *layout;
        }
        else
        {
            assert(false);
        }

        tone_mapping_pipeline_ =
            vkrndr::compute_pipeline_builder_t{backend_->device(),
                vkrndr::pipeline_layout_builder_t{backend_->device()}
                    .add_descriptor_set_layout(
                        tone_mapping_descriptor_set_layout_)
                    .build()}
                .with_shader(as_pipeline_shader(*shader))
                .build();
    }

    {
        vkglsl::shader_set_t shader_set{enable_shader_debug_symbols,
            enable_shader_optimization};

        auto shader{add_shader_module_from_path(shader_set,
            backend_->device(),
            VK_SHADER_STAGE_COMPUTE_BIT,
            "fxaa.comp")};
        assert(shader);
        [[maybe_unused]] boost::scope::defer_guard const destroy_shd{
            [this, shd = &shader.value()]()
            { destroy(&backend_->device(), shd); }};

        if (auto const layout{
                descriptor_set_layout(shader_set, backend_->device(), 0)})
        {
            fxaa_descriptor_set_layout_ = *layout;
        }
        else
        {
            assert(false);
        }

        fxaa_pipeline_ = vkrndr::compute_pipeline_builder_t{backend_->device(),
            vkrndr::pipeline_layout_builder_t{backend_->device()}
                .add_descriptor_set_layout(fxaa_descriptor_set_layout_)
                .build()}
                             .with_shader(as_pipeline_shader(*shader))
                             .build();
    }

    for (auto& data : cppext::as_span(frame_data_))
    {
        vkrndr::create_descriptor_sets(backend_->device(),
            backend_->descriptor_pool(),
            cppext::as_span(tone_mapping_descriptor_set_layout_),
            cppext::as_span(data.tone_mapping_descriptor_set));

        vkrndr::create_descriptor_sets(backend_->device(),
            backend_->descriptor_pool(),
            cppext::as_span(fxaa_descriptor_set_layout_),
            cppext::as_span(data.fxaa_descriptor_set));
    }
}

galileo::postprocess_shader_t::~postprocess_shader_t()
{
    for (auto& data : cppext::as_span(frame_data_))
    {
        vkrndr::free_descriptor_sets(backend_->device(),
            backend_->descriptor_pool(),
            cppext::as_span(data.fxaa_descriptor_set));

        vkrndr::free_descriptor_sets(backend_->device(),
            backend_->descriptor_pool(),
            cppext::as_span(data.tone_mapping_descriptor_set));
    }

    destroy(&backend_->device(), &fxaa_pipeline_);

    destroy(&backend_->device(), &tone_mapping_pipeline_);

    vkDestroyDescriptorSetLayout(backend_->device().logical,
        fxaa_descriptor_set_layout_,
        nullptr);

    vkDestroyDescriptorSetLayout(backend_->device().logical,
        tone_mapping_descriptor_set_layout_,
        nullptr);

    destroy(&backend_->device(), &intermediate_image_);

    vkDestroySampler(backend_->device().logical, sampler_, nullptr);
}

void galileo::postprocess_shader_t::draw(VkCommandBuffer command_buffer,
    vkrndr::image_t const& color_image,
    vkrndr::image_t const& target_image)
{
    frame_data_.cycle();

    {
        auto const barrier{vkrndr::to_layout(
            vkrndr::with_access(
                vkrndr::on_stage(vkrndr::image_barrier(intermediate_image_),
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT),
                VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT),
            VK_IMAGE_LAYOUT_GENERAL)};
        vkrndr::wait_for(command_buffer, {}, {}, cppext::as_span(barrier));
    }

    {
        VKRNDR_IF_DEBUG_UTILS(
            [[maybe_unused]] vkrndr::command_buffer_scope_t const cb_scope{
                command_buffer,
                "Tone Mapping"});
        update_descriptor_set(backend_->device(),
            frame_data_->tone_mapping_descriptor_set,
            vkrndr::combined_sampler_descriptor(sampler_, color_image),
            vkrndr::storage_image_descriptor(intermediate_image_));

        vkrndr::bind_pipeline(command_buffer,
            tone_mapping_pipeline_,
            0,
            cppext::as_span(frame_data_->tone_mapping_descriptor_set));

        vkCmdDispatch(command_buffer,
            static_cast<uint32_t>(std::ceil(
                cppext::as_fp(intermediate_image_.extent.width) / 16.0f)),
            static_cast<uint32_t>(std::ceil(
                cppext::as_fp(intermediate_image_.extent.height) / 16.0f)),
            1);
    }

    {
        auto const barrier{vkrndr::with_layout(
            vkrndr::with_access(
                vkrndr::on_stage(vkrndr::image_barrier(intermediate_image_),
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT),
                VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                VK_ACCESS_2_SHADER_SAMPLED_READ_BIT),
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)};
        vkrndr::wait_for(command_buffer, {}, {}, cppext::as_span(barrier));
    }

    {
        VKRNDR_IF_DEBUG_UTILS(
            [[maybe_unused]] vkrndr::command_buffer_scope_t const cb_scope{
                command_buffer,
                "FXAA"});
        update_descriptor_set(backend_->device(),
            frame_data_->fxaa_descriptor_set,
            vkrndr::combined_sampler_descriptor(sampler_, intermediate_image_),
            vkrndr::storage_image_descriptor(target_image));

        vkrndr::bind_pipeline(command_buffer,
            fxaa_pipeline_,
            0,
            cppext::as_span(frame_data_->fxaa_descriptor_set));

        vkCmdDispatch(command_buffer,
            static_cast<uint32_t>(
                std::ceil(cppext::as_fp(target_image.extent.width) / 16.0f)),
            static_cast<uint32_t>(
                std::ceil(cppext::as_fp(target_image.extent.height) / 16.0f)),
            1);
    }
}

void galileo::postprocess_shader_t::resize(uint32_t width, uint32_t height)
{
    destroy(&backend_->device(), &intermediate_image_);
    intermediate_image_ = create_intermediate_image(*backend_,
        vkrndr::to_2d_extent(width, height));
}
