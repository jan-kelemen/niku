#include <resolve_shader.hpp>

#include <config.hpp>

#include <cppext_container.hpp>
#include <cppext_cycled_buffer.hpp>
#include <cppext_numeric.hpp>

#include <vkglsl_shader_set.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_compute_pipeline_builder.hpp>
#include <vkrndr_debug_utils.hpp>
#include <vkrndr_descriptors.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_pipeline.hpp>
#include <vkrndr_pipeline_layout_builder.hpp>
#include <vkrndr_shader_module.hpp>
#include <vkrndr_utility.hpp>

#include <boost/scope/defer.hpp>

#include <volk.h>

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <span>
#include <utility>

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
        VkDescriptorImageInfo const target_image_info,
        VkDescriptorImageInfo const bright_image_info)
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

        VkWriteDescriptorSet bright_image_uniform_write{};
        bright_image_uniform_write.sType =
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        bright_image_uniform_write.dstSet = descriptor_set;
        bright_image_uniform_write.dstBinding = 2;
        bright_image_uniform_write.dstArrayElement = 0;
        bright_image_uniform_write.descriptorType =
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        bright_image_uniform_write.descriptorCount = 1;
        bright_image_uniform_write.pImageInfo = &bright_image_info;

        std::array const descriptor_writes{combined_sampler_uniform_write,
            target_image_uniform_write,
            bright_image_uniform_write};

        vkUpdateDescriptorSets(device,
            vkrndr::count_cast(descriptor_writes.size()),
            descriptor_writes.data(),
            0,
            nullptr);
    }

    [[nodiscard]] VkSampler create_sampler(vkrndr::device_t const& device)
    {
        VkPhysicalDeviceProperties properties; // NOLINT
        vkGetPhysicalDeviceProperties(device, &properties);

        VkSamplerCreateInfo sampler_info{};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = VK_FILTER_NEAREST;
        sampler_info.minFilter = VK_FILTER_NEAREST;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.anisotropyEnable = VK_TRUE;
        sampler_info.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
        sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        sampler_info.unnormalizedCoordinates = VK_FALSE;
        sampler_info.compareEnable = VK_FALSE;
        sampler_info.compareOp = VK_COMPARE_OP_NEVER;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler_info.mipLodBias = 0.0f;
        sampler_info.minLod = 0.0f;
        sampler_info.maxLod = 1.0f;

        VkSampler rv; // NOLINT
        vkrndr::check_result(
            vkCreateSampler(device, &sampler_info, nullptr, &rv));

        return rv;
    }
} // namespace

gltfviewer::resolve_shader_t::resolve_shader_t(vkrndr::backend_t& backend)
    : backend_{&backend}
    , combined_sampler_{create_sampler(backend_->device())}
    , descriptor_sets_{backend_->frames_in_flight(),
          backend_->frames_in_flight()}
{
    vkglsl::shader_set_t shader_set{enable_shader_debug_symbols,
        enable_shader_optimization};

    auto shader{add_shader_module_from_path(shader_set,
        backend_->device(),
        VK_SHADER_STAGE_COMPUTE_BIT,
        "resolve.comp")};
    assert(shader);
    [[maybe_unused]] boost::scope::defer_guard const destroy_shd{
        [this, &shd = shader.value()]() { destroy(backend_->device(), shd); }};

    if (auto const layout{
            descriptor_set_layout(shader_set, backend_->device(), 0)})
    {
        descriptor_set_layout_ = *layout;
    }
    else
    {
        assert(false);
    }

    auto const sample_count{cppext::narrow<uint32_t>(
        std::to_underlying(backend_->device().max_msaa_samples))};
    VkSpecializationMapEntry const sample_specialization{.constantID = 0,
        .offset = 0,
        .size = sizeof(sample_count)};
    VkSpecializationInfo const specialization{.mapEntryCount = 1,
        .pMapEntries = &sample_specialization,
        .dataSize = sizeof(sample_specialization),
        .pData = &sample_count};

    pipeline_layout_ = vkrndr::pipeline_layout_builder_t{backend_->device()}
                           .add_descriptor_set_layout(descriptor_set_layout_)
                           .build();
    pipeline_ =
        vkrndr::compute_pipeline_builder_t{backend_->device(), pipeline_layout_}
            .with_shader(as_pipeline_shader(*shader, &specialization))
            .build();

    for (auto& set : cppext::as_span(descriptor_sets_))
    {
        vkrndr::check_result(allocate_descriptor_sets(backend_->device(),
            backend_->descriptor_pool(),
            cppext::as_span(descriptor_set_layout_),
            cppext::as_span(set)));
    }
}

gltfviewer::resolve_shader_t::~resolve_shader_t()
{
    vkDestroySampler(backend_->device(), combined_sampler_, nullptr);

    destroy(backend_->device(), pipeline_);
    destroy(backend_->device(), pipeline_layout_);

    free_descriptor_sets(backend_->device(),
        backend_->descriptor_pool(),
        cppext::as_span(descriptor_sets_));

    vkDestroyDescriptorSetLayout(backend_->device(),
        descriptor_set_layout_,
        nullptr);
}

void gltfviewer::resolve_shader_t::draw(VkCommandBuffer command_buffer,
    vkrndr::image_t const& color_image,
    vkrndr::image_t const& target_image,
    vkrndr::image_t const& bright_image)
{
    descriptor_sets_.cycle();

    VKRNDR_IF_DEBUG_UTILS(
        [[maybe_unused]] vkrndr::command_buffer_scope_t const cb_scope{
            command_buffer,
            "Resolve"});
    update_descriptor_set(backend_->device(),
        *descriptor_sets_,
        vkrndr::combined_sampler_descriptor(combined_sampler_, color_image),
        vkrndr::storage_image_descriptor(target_image),
        vkrndr::storage_image_descriptor(bright_image));

    vkrndr::bind_pipeline(command_buffer,
        pipeline_,
        0,
        cppext::as_span(descriptor_sets_.current()));

    vkCmdDispatch(command_buffer,
        static_cast<uint32_t>(
            std::ceil(cppext::as_fp(target_image.extent.width) / 16.0f)),
        static_cast<uint32_t>(
            std::ceil(cppext::as_fp(target_image.extent.height) / 16.0f)),
        1);
}
