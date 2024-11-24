#include <weighted_blend_shader.hpp>

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

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <span>

// IWYU pragma: no_include <expected>
// IWYU pragma: no_include <filesystem>
// IWYU pragma: no_include <memory>
// IWYU pragma: no_include <system_error>

namespace
{
    struct [[nodiscard]] push_constants_t final
    {
        float weight;
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

    void update_descriptor_set(vkrndr::device_t const& device,
        VkDescriptorSet const descriptor_set,
        VkDescriptorImageInfo target_image_info,
        VkDescriptorImageInfo other_image_info)
    {
        VkWriteDescriptorSet sampler_write{};
        sampler_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        sampler_write.dstSet = descriptor_set;
        sampler_write.dstBinding = 0;
        sampler_write.dstArrayElement = 0;
        sampler_write.descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        sampler_write.descriptorCount = 1;
        sampler_write.pImageInfo = &other_image_info;

        VkWriteDescriptorSet target_write{};
        target_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        target_write.dstSet = descriptor_set;
        target_write.dstBinding = 1;
        target_write.dstArrayElement = 0;
        target_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        target_write.descriptorCount = 1;
        target_write.pImageInfo = &target_image_info;

        std::array const descriptor_writes{sampler_write, target_write};

        vkUpdateDescriptorSets(device.logical,
            vkrndr::count_cast(descriptor_writes.size()),
            descriptor_writes.data(),
            0,
            nullptr);
    }
} // namespace

gltfviewer::weighted_blend_shader_t::weighted_blend_shader_t(
    vkrndr::backend_t& backend)
    : backend_{&backend}
    , bilinear_sampler_{create_sampler(backend_->device())}
    , frame_data_{backend_->frames_in_flight(), backend_->frames_in_flight()}
{
    vkglsl::shader_set_t set;
    auto shader{vkglsl::add_shader_binary_from_path(set,
        backend_->device(),
        VK_SHADER_STAGE_COMPUTE_BIT,
        "weighted_blend.comp.spv")};
    assert(shader);
    [[maybe_unused]] boost::scope::defer_guard const destroy_shd{
        [this, shd = &shader.value()]() { destroy(&backend_->device(), shd); }};

    auto layout{vkglsl::descriptor_set_layout(set, backend_->device(), 0)};
    assert(layout);
    descriptor_layout_ = *layout;

    pipeline_ = vkrndr::compute_pipeline_builder_t{backend_->device(),
        vkrndr::pipeline_layout_builder_t{backend_->device()}
            .add_push_constants<push_constants_t>(VK_SHADER_STAGE_COMPUTE_BIT)
            .add_descriptor_set_layout(descriptor_layout_)
            .build()}
                    .with_shader(as_pipeline_shader(*shader))
                    .build();

    for (frame_data_t& data : frame_data_.as_span())
    {
        auto ds{std::span{&data.descriptor_, 1}};

        vkrndr::create_descriptor_sets(backend_->device(),
            backend_->descriptor_pool(),
            std::span{&descriptor_layout_, 1},
            ds);
    }
}

gltfviewer::weighted_blend_shader_t::~weighted_blend_shader_t()
{
    for (frame_data_t& data : frame_data_.as_span())
    {
        vkrndr::free_descriptor_sets(backend_->device(),
            backend_->descriptor_pool(),
            std::span{&data.descriptor_, 1});
    }

    destroy(&backend_->device(), &pipeline_);

    vkDestroyDescriptorSetLayout(backend_->device().logical,
        descriptor_layout_,
        nullptr);

    vkDestroySampler(backend_->device().logical, bilinear_sampler_, nullptr);
}

void gltfviewer::weighted_blend_shader_t::draw(float const bias,
    VkCommandBuffer command_buffer,
    vkrndr::image_t const& target_image,
    vkrndr::image_t const& other_image)
{
    frame_data_.cycle();

    [[maybe_unused]] vkrndr::command_buffer_scope_t const cb_scope{
        command_buffer,
        "Weighted blend"};

    update_descriptor_set(backend_->device(),
        frame_data_->descriptor_,
        vkrndr::storage_image_descriptor(target_image),
        vkrndr::combined_sampler_descriptor(bilinear_sampler_,
            other_image,
            VK_IMAGE_LAYOUT_GENERAL));

    vkrndr::bind_pipeline(command_buffer,
        pipeline_,
        0,
        std::span{&frame_data_->descriptor_, 1});

    push_constants_t pc{.weight = bias};
    vkCmdPushConstants(command_buffer,
        *pipeline_.layout,
        VK_SHADER_STAGE_COMPUTE_BIT,
        0,
        sizeof(pc),
        &pc);

    vkCmdDispatch(command_buffer,
        static_cast<uint32_t>(
            std::ceil(cppext::as_fp(target_image.extent.width) / 16.0f)),
        static_cast<uint32_t>(
            std::ceil(cppext::as_fp(target_image.extent.height) / 16.0f)),
        1);
}
