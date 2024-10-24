#include <postprocess_shader.hpp>

#include <cppext_cycled_buffer.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_descriptors.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_pipeline.hpp>
#include <vkrndr_render_pass.hpp>
#include <vkrndr_shader_module.hpp>
#include <vkrndr_utility.hpp>

#include <volk.h>

#include <array>
#include <cstdint>
#include <span>

// IWYU pragma: no_include <filesystem>
// IWYU pragma: no_include <memory>
// IWYU pragma: no_include <string_view>
// IWYU pragma: no_forward_declare VkDescriptorSet_T

namespace
{
    struct [[nodiscard]] push_constants_t final
    {
        float gamma;
        float exposure;
    };

    [[nodiscard]] VkDescriptorSetLayout create_descriptor_set_layout(
        vkrndr::device_t const& device)
    {
        VkDescriptorSetLayoutBinding combined_sampler_binding{};
        combined_sampler_binding.binding = 0;
        combined_sampler_binding.descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        combined_sampler_binding.descriptorCount = 1;
        combined_sampler_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        return vkrndr::create_descriptor_set_layout(device,
            std::span{&combined_sampler_binding, 1});
    }

    void bind_descriptor_set(vkrndr::device_t const& device,
        VkDescriptorSet const descriptor_set,
        VkDescriptorImageInfo const combined_sampler_info)
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

        std::array const descriptor_writes{combined_sampler_uniform_write};

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
            vkCreateSampler(device.logical, &sampler_info, nullptr, &rv));

        return rv;
    }
} // namespace

gltfviewer::postprocess_shader_t::postprocess_shader_t(
    vkrndr::backend_t& backend)
    : backend_{&backend}
    , combined_sampler_{create_sampler(backend_->device())}
    , descriptor_set_layout_{create_descriptor_set_layout(backend_->device())}
    , descriptor_sets_{backend_->frames_in_flight(),
          backend_->frames_in_flight()}
{
    auto vertex_shader{vkrndr::create_shader_module(backend_->device(),
        "fullscreen.vert.spv",
        VK_SHADER_STAGE_VERTEX_BIT,
        "main")};
    auto fragment_shader{vkrndr::create_shader_module(backend_->device(),
        "postprocess.frag.spv",
        VK_SHADER_STAGE_FRAGMENT_BIT,
        "main")};

    uint32_t sample_count = backend_->device().max_msaa_samples;
    VkSpecializationMapEntry const sample_specialization{.constantID = 0,
        .offset = 0,
        .size = sizeof(sample_count)};
    VkSpecializationInfo const fragment_specialization{.mapEntryCount = 1,
        .pMapEntries = &sample_specialization,
        .dataSize = sizeof(sample_specialization),
        .pData = &sample_count};

    pipeline_ = vkrndr::pipeline_builder_t{backend_->device(),
        vkrndr::pipeline_layout_builder_t{backend_->device()}
            .add_descriptor_set_layout(descriptor_set_layout_)
            .add_push_constants<push_constants_t>(VK_SHADER_STAGE_FRAGMENT_BIT)
            .build()}
                    .add_shader(as_pipeline_shader(vertex_shader))
                    .add_shader(as_pipeline_shader(fragment_shader,
                        &fragment_specialization))
                    .add_color_attachment(backend_->image_format())
                    .with_culling(VK_CULL_MODE_FRONT_BIT,
                        VK_FRONT_FACE_COUNTER_CLOCKWISE)
                    .build();

    destroy(&backend_->device(), &vertex_shader);
    destroy(&backend_->device(), &fragment_shader);

    for (auto& set : descriptor_sets_.as_span())
    {
        vkrndr::create_descriptor_sets(backend_->device(),
            backend_->descriptor_pool(),
            std::span{&descriptor_set_layout_, 1},
            std::span{&set, 1});
    }
}

gltfviewer::postprocess_shader_t::~postprocess_shader_t()
{
    vkDestroySampler(backend_->device().logical, combined_sampler_, nullptr);

    destroy(&backend_->device(), &pipeline_);

    vkrndr::free_descriptor_sets(backend_->device(),
        backend_->descriptor_pool(),
        descriptor_sets_.as_span());

    vkDestroyDescriptorSetLayout(backend_->device().logical,
        descriptor_set_layout_,
        nullptr);
}

void gltfviewer::postprocess_shader_t::update(float const gamma,
    float const exposure)
{
    gamma_ = gamma;
    exposure_ = exposure;
}

void gltfviewer::postprocess_shader_t::draw(VkCommandBuffer command_buffer,
    vkrndr::image_t const& color_image,
    vkrndr::image_t const& target_image)
{
    descriptor_sets_.cycle();

    bind_descriptor_set(backend_->device(),
        *descriptor_sets_,
        vkrndr::combined_sampler_descriptor(combined_sampler_, color_image));

    push_constants_t const pc{.gamma = gamma_, .exposure = exposure_};
    vkCmdPushConstants(command_buffer,
        *pipeline_.layout,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(push_constants_t),
        &pc);

    vkrndr::render_pass_t color_render_pass;
    color_render_pass.with_color_attachment(VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        VK_ATTACHMENT_STORE_OP_STORE,
        target_image.view);

    [[maybe_unused]] auto guard{
        color_render_pass.begin(command_buffer, {{0, 0}, target_image.extent})};

    vkrndr::bind_pipeline(command_buffer,
        pipeline_,
        0,
        std::span{&descriptor_sets_.current(), 1});

    vkCmdDraw(command_buffer, 3, 1, 0, 0);
}
