#include <postprocess_shader.hpp>

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
    struct [[nodiscard]] push_constants_t final
    {
        uint32_t color_conversion;
        uint32_t tone_mapping;
    };

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
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
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

        vkUpdateDescriptorSets(device,
            vkrndr::count_cast(descriptor_writes),
            descriptor_writes.data(),
            0,
            nullptr);
    }
} // namespace

gltfviewer::postprocess_shader_t::postprocess_shader_t(
    vkrndr::backend_t& backend)
    : backend_{&backend}
    , descriptor_sets_{backend_->frames_in_flight(),
          backend_->frames_in_flight()}
{
    vkglsl::shader_set_t shader_set{enable_shader_debug_symbols,
        enable_shader_optimization};

    auto shader{add_shader_module_from_path(shader_set,
        backend_->device(),
        VK_SHADER_STAGE_COMPUTE_BIT,
        "postprocess.comp")};
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

    pipeline_layout_ =
        vkrndr::pipeline_layout_builder_t{backend_->device()}
            .add_descriptor_set_layout(descriptor_set_layout_)
            .add_push_constants<push_constants_t>(VK_SHADER_STAGE_COMPUTE_BIT)
            .build();

    pipeline_ =
        vkrndr::compute_pipeline_builder_t{backend_->device(), pipeline_layout_}
            .with_shader(as_pipeline_shader(*shader))
            .build();

    for (auto& set : cppext::as_span(descriptor_sets_))
    {
        vkrndr::check_result(allocate_descriptor_sets(backend_->device(),
            backend_->descriptor_pool(),
            cppext::as_span(descriptor_set_layout_),
            cppext::as_span(set)));
    }
}

gltfviewer::postprocess_shader_t::~postprocess_shader_t()
{
    destroy(backend_->device(), pipeline_);
    destroy(backend_->device(), pipeline_layout_);

    free_descriptor_sets(backend_->device(),
        backend_->descriptor_pool(),
        cppext::as_span(descriptor_sets_));

    vkDestroyDescriptorSetLayout(backend_->device(),
        descriptor_set_layout_,
        nullptr);
}

void gltfviewer::postprocess_shader_t::draw(bool const color_conversion,
    bool const tone_mapping,
    VkCommandBuffer command_buffer,
    vkrndr::image_t const& color_image,
    vkrndr::image_t const& target_image)
{
    descriptor_sets_.cycle();

    VKRNDR_IF_DEBUG_UTILS(
        [[maybe_unused]] vkrndr::command_buffer_scope_t const cb_scope{
            command_buffer,
            "Color Conversion & Tone Mapping"});
    update_descriptor_set(backend_->device(),
        *descriptor_sets_,
        vkrndr::storage_image_descriptor(color_image),
        vkrndr::storage_image_descriptor(target_image));

    push_constants_t const pc{
        .color_conversion = static_cast<uint32_t>(color_conversion),
        .tone_mapping = static_cast<uint32_t>(tone_mapping)};
    vkCmdPushConstants(command_buffer,
        pipeline_.layout,
        VK_SHADER_STAGE_COMPUTE_BIT,
        0,
        sizeof(push_constants_t),
        &pc);

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
