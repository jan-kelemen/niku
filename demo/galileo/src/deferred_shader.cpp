#include <deferred_shader.hpp>

#include <config.hpp>
#include <gbuffer.hpp>

#include <cppext_container.hpp>
#include <cppext_cycled_buffer.hpp>

#include <vkglsl_shader_set.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_descriptors.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_graphics_pipeline_builder.hpp>
#include <vkrndr_pipeline.hpp>
#include <vkrndr_pipeline_layout_builder.hpp>
#include <vkrndr_shader_module.hpp>
#include <vkrndr_utility.hpp>

#include <boost/scope/defer.hpp>

#include <array>
#include <cassert>

// IWYU pragma: no_include <expected>
// IWYU pragma: no_include <filesystem>
// IWYU pragma: no_include <memory>
// IWYU pragma: no_include <optional>
// IWYU pragma: no_include <span>
// IWYU pragma: no_include <system_error>

namespace
{
    [[nodiscard]] VkDescriptorSetLayout create_descriptor_set_layout(
        vkrndr::device_t const& device)
    {
        VkDescriptorSetLayoutBinding position_binding{};
        position_binding.binding = 0;
        position_binding.descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        position_binding.descriptorCount = 1;
        position_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding normal_binding{};
        normal_binding.binding = 1;
        normal_binding.descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        normal_binding.descriptorCount = 1;
        normal_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding albedo_binding{};
        albedo_binding.binding = 2;
        albedo_binding.descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        albedo_binding.descriptorCount = 1;
        albedo_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array const bindings{position_binding,
            normal_binding,
            albedo_binding};

        return vkrndr::create_descriptor_set_layout(device, bindings).value();
    }

    void update_descriptor_set(vkrndr::device_t const& device,
        VkDescriptorSet const descriptor_set,
        VkSampler const sampler,
        galileo::gbuffer_t& gbuffer)
    {
        auto pi{vkrndr::combined_sampler_descriptor(sampler,
            gbuffer.position_image())};
        VkWriteDescriptorSet position_write{};
        position_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        position_write.dstSet = descriptor_set;
        position_write.dstBinding = 0;
        position_write.dstArrayElement = 0;
        position_write.descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        position_write.descriptorCount = 1;
        position_write.pImageInfo = &pi;

        auto ni{vkrndr::combined_sampler_descriptor(sampler,
            gbuffer.normal_image())};
        VkWriteDescriptorSet normal_write{};
        normal_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        normal_write.dstSet = descriptor_set;
        normal_write.dstBinding = 1;
        normal_write.dstArrayElement = 0;
        normal_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        normal_write.descriptorCount = 1;
        normal_write.pImageInfo = &ni;

        auto ai{vkrndr::combined_sampler_descriptor(sampler,
            gbuffer.albedo_image())};
        VkWriteDescriptorSet albedo_write{};
        albedo_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        albedo_write.dstSet = descriptor_set;
        albedo_write.dstBinding = 2;
        albedo_write.dstArrayElement = 0;
        albedo_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        albedo_write.descriptorCount = 1;
        albedo_write.pImageInfo = &ai;

        std::array const descriptor_writes{position_write,
            normal_write,
            albedo_write};

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
            vkCreateSampler(device, &sampler_info, nullptr, &rv));

        return rv;
    }
} // namespace

galileo::deferred_shader_t::deferred_shader_t(vkrndr::backend_t& backend,
    VkDescriptorSetLayout frame_info_layout)
    : backend_{&backend}
    , descriptor_set_layout_{create_descriptor_set_layout(backend_->device())}
    , sampler_{create_sampler(backend_->device())}
    , frame_data_{backend_->frames_in_flight(), backend_->frames_in_flight()}
{
    vkglsl::shader_set_t shader_set{enable_shader_debug_symbols,
        enable_shader_optimization};

    auto vertex_shader{add_shader_module_from_path(shader_set,
        backend_->device(),
        VK_SHADER_STAGE_VERTEX_BIT,
        "fullscreen.vert")};
    assert(vertex_shader);
    [[maybe_unused]] boost::scope::defer_guard const destroy_vtx{
        [this, &shd = vertex_shader.value()]()
        { destroy(backend_->device(), shd); }};

    auto fragment_shader{add_shader_module_from_path(shader_set,
        backend_->device(),
        VK_SHADER_STAGE_FRAGMENT_BIT,
        "deferred.frag")};
    assert(fragment_shader);
    [[maybe_unused]] boost::scope::defer_guard const destroy_frag{
        [this, &shd = fragment_shader.value()]()
        { destroy(backend_->device(), shd); }};

    pipeline_ = vkrndr::graphics_pipeline_builder_t{backend_->device(),
        vkrndr::pipeline_layout_builder_t{backend_->device()}
            .add_descriptor_set_layout(frame_info_layout)
            .add_descriptor_set_layout(descriptor_set_layout_)
            .build()}
                    .add_shader(as_pipeline_shader(*vertex_shader))
                    .add_shader(as_pipeline_shader(*fragment_shader))
                    .add_color_attachment(VK_FORMAT_R16G16B16A16_SFLOAT)
                    .with_culling(VK_CULL_MODE_FRONT_BIT,
                        VK_FRONT_FACE_COUNTER_CLOCKWISE)
                    .build();

    for (auto& data : cppext::as_span(frame_data_))
    {
        vkrndr::check_result(allocate_descriptor_sets(backend_->device(),
            backend_->descriptor_pool(),
            cppext::as_span(descriptor_set_layout_),
            cppext::as_span(data.descriptor_set)));
    }
}

galileo::deferred_shader_t::~deferred_shader_t()
{
    for (auto& data : cppext::as_span(frame_data_))
    {
        free_descriptor_sets(backend_->device(),
            backend_->descriptor_pool(),
            cppext::as_span(data.descriptor_set));
    }

    destroy(&backend_->device(), &pipeline_);

    vkDestroySampler(backend_->device(), sampler_, nullptr);

    vkDestroyDescriptorSetLayout(backend_->device(),
        descriptor_set_layout_,
        nullptr);
}

VkPipelineLayout galileo::deferred_shader_t::pipeline_layout() const
{
    return *pipeline_.layout;
}

void galileo::deferred_shader_t::draw(VkCommandBuffer command_buffer,
    gbuffer_t& gbuffer)
{
    frame_data_.cycle();

    update_descriptor_set(backend_->device(),
        frame_data_->descriptor_set,
        sampler_,
        gbuffer);

    vkCmdBindDescriptorSets(command_buffer,
        pipeline_.type,
        *pipeline_.layout,
        1,
        1,
        &frame_data_->descriptor_set,
        0,
        nullptr);

    vkrndr::bind_pipeline(command_buffer, pipeline_);

    vkCmdDraw(command_buffer, 3, 1, 0, 0);
}
