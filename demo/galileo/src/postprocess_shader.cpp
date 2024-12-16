#include <postprocess_shader.hpp>

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
    , descriptor_sets_{backend_->frames_in_flight(),
          backend_->frames_in_flight()}
{
    vkglsl::shader_set_t shader_set{true, false};

    auto shader{add_shader_module_from_path(shader_set,
        backend_->device(),
        VK_SHADER_STAGE_COMPUTE_BIT,
        "postprocess.comp")};
    assert(shader);
    [[maybe_unused]] boost::scope::defer_guard const destroy_shd{
        [this, shd = &shader.value()]() { destroy(&backend_->device(), shd); }};

    if (auto const layout{
            descriptor_set_layout(shader_set, backend_->device(), 0)})
    {
        descriptor_set_layout_ = *layout;
    }
    else
    {
        assert(false);
    }

    pipeline_ = vkrndr::compute_pipeline_builder_t{backend_->device(),
        vkrndr::pipeline_layout_builder_t{backend_->device()}
            .add_descriptor_set_layout(descriptor_set_layout_)
            .build()}
                    .with_shader(as_pipeline_shader(*shader))
                    .build();

    for (auto& set : descriptor_sets_.as_span())
    {
        vkrndr::create_descriptor_sets(backend_->device(),
            backend_->descriptor_pool(),
            std::span{&descriptor_set_layout_, 1},
            std::span{&set, 1});
    }
}

galileo::postprocess_shader_t::~postprocess_shader_t()
{
    destroy(&backend_->device(), &pipeline_);

    vkrndr::free_descriptor_sets(backend_->device(),
        backend_->descriptor_pool(),
        descriptor_sets_.as_span());

    vkDestroyDescriptorSetLayout(backend_->device().logical,
        descriptor_set_layout_,
        nullptr);

    vkDestroySampler(backend_->device().logical, sampler_, nullptr);
}

void galileo::postprocess_shader_t::draw(VkCommandBuffer command_buffer,
    vkrndr::image_t const& color_image,
    vkrndr::image_t const& target_image)
{
    descriptor_sets_.cycle();

    [[maybe_unused]] vkrndr::command_buffer_scope_t const cb_scope{
        command_buffer,
        "FXAA & Tone Mapping"};
    update_descriptor_set(backend_->device(),
        *descriptor_sets_,
        vkrndr::combined_sampler_descriptor(sampler_, color_image),
        vkrndr::storage_image_descriptor(target_image));

    vkrndr::bind_pipeline(command_buffer,
        pipeline_,
        0,
        std::span{&descriptor_sets_.current(), 1});

    vkCmdDispatch(command_buffer,
        static_cast<uint32_t>(
            std::ceil(cppext::as_fp(target_image.extent.width) / 16.0f)),
        static_cast<uint32_t>(
            std::ceil(cppext::as_fp(target_image.extent.height) / 16.0f)),
        1);
}
