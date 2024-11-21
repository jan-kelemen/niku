#include <weighted_oit_shader.hpp>

#include <render_graph.hpp>

#include <cppext_numeric.hpp>

#include <vkglsl_shader_set.hpp>

#include <vkgltf_model.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_commands.hpp>
#include <vkrndr_debug_utils.hpp>
#include <vkrndr_descriptors.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_pipeline.hpp>
#include <vkrndr_render_pass.hpp>
#include <vkrndr_shader_module.hpp>
#include <vkrndr_utility.hpp>

#include <boost/scope/defer.hpp>

#include <volk.h>

#include <array>
#include <cassert>
#include <filesystem>
#include <functional>
#include <span>
#include <string_view>
#include <utility>

// IWYU pragma: no_include <expected>
// IWYU pragma: no_include <memory>
// IWYU pragma: no_include <optional>
// IWYU pragma: no_include <type_traits>
// IWYU pragma: no_include <system_error>
// IWYU pragma: no_forward_declare VkDescriptorSet_T

namespace
{
    [[nodiscard]] VkDescriptorSetLayout create_descriptor_set_layout(
        vkrndr::device_t const& device)
    {
        VkDescriptorSetLayoutBinding accumulation_sampler_binding{};
        accumulation_sampler_binding.binding = 0;
        accumulation_sampler_binding.descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        accumulation_sampler_binding.descriptorCount = 1;
        accumulation_sampler_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding reveal_sampler_binding{};
        reveal_sampler_binding.binding = 1;
        reveal_sampler_binding.descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        reveal_sampler_binding.descriptorCount = 1;
        reveal_sampler_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        return vkrndr::create_descriptor_set_layout(device,
            std::array{accumulation_sampler_binding, reveal_sampler_binding});
    }

    void update_descriptor_set(vkrndr::device_t const& device,
        VkDescriptorSet const descriptor_set,
        VkDescriptorImageInfo const accumulation_sampler_info,
        VkDescriptorImageInfo const reveal_sampler_info)
    {
        VkWriteDescriptorSet accumulation_sampler_uniform_write{};
        accumulation_sampler_uniform_write.sType =
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        accumulation_sampler_uniform_write.dstSet = descriptor_set;
        accumulation_sampler_uniform_write.dstBinding = 0;
        accumulation_sampler_uniform_write.dstArrayElement = 0;
        accumulation_sampler_uniform_write.descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        accumulation_sampler_uniform_write.descriptorCount = 1;
        accumulation_sampler_uniform_write.pImageInfo =
            &accumulation_sampler_info;

        VkWriteDescriptorSet reveal_sampler_uniform_write{};
        reveal_sampler_uniform_write.sType =
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        reveal_sampler_uniform_write.dstSet = descriptor_set;
        reveal_sampler_uniform_write.dstBinding = 1;
        reveal_sampler_uniform_write.dstArrayElement = 0;
        reveal_sampler_uniform_write.descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        reveal_sampler_uniform_write.descriptorCount = 1;
        reveal_sampler_uniform_write.pImageInfo = &reveal_sampler_info;

        std::array const descriptor_writes{accumulation_sampler_uniform_write,
            reveal_sampler_uniform_write};

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
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
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

gltfviewer::weighted_oit_shader_t::weighted_oit_shader_t(
    vkrndr::backend_t& backend)
    : backend_{&backend}
    , descriptor_set_layout_{create_descriptor_set_layout(backend_->device())}
    , accumulation_sampler_{create_sampler(backend_->device())}
    , reveal_sampler_{create_sampler(backend_->device())}
{
    vkrndr::create_descriptor_sets(backend_->device(),
        backend_->descriptor_pool(),
        std::span{&descriptor_set_layout_, 1},
        std::span{&descriptor_set_, 1});
}

gltfviewer::weighted_oit_shader_t::~weighted_oit_shader_t()
{
    destroy(&backend_->device(), &composition_pipeline_);
    destroy(&backend_->device(), &pbr_pipeline_);

    vkDestroySampler(backend_->device().logical, reveal_sampler_, nullptr);
    vkDestroySampler(backend_->device().logical,
        accumulation_sampler_,
        nullptr);

    vkrndr::free_descriptor_sets(backend_->device(),
        backend_->descriptor_pool(),
        std::span{&descriptor_set_, 1});

    vkDestroyDescriptorSetLayout(backend_->device().logical,
        descriptor_set_layout_,
        nullptr);

    destroy(&backend_->device(), &reveal_image_);
    destroy(&backend_->device(), &accumulation_image_);
}

VkPipelineLayout gltfviewer::weighted_oit_shader_t::pipeline_layout() const
{
    if (pbr_pipeline_.pipeline)
    {
        return *pbr_pipeline_.layout;
    }

    return VK_NULL_HANDLE;
}

void gltfviewer::weighted_oit_shader_t::draw(render_graph_t const& graph,
    VkCommandBuffer command_buffer,
    vkrndr::image_t const& color_image,
    vkrndr::image_t const& depth_buffer)
{
    [[maybe_unused]] vkrndr::command_buffer_scope_t const cb_scope{
        command_buffer,
        "PBR Transparent"};

    {
        [[maybe_unused]] vkrndr::command_buffer_scope_t const
            geometry_pass_scope{command_buffer, "Geometry"};

        vkrndr::wait_for_color_attachment_write(accumulation_image_.image,
            command_buffer);
        vkrndr::wait_for_color_attachment_write(reveal_image_.image,
            command_buffer);

        vkrndr::render_pass_t color_render_pass;
        color_render_pass.with_color_attachment(VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            accumulation_image_.view,
            VkClearValue{.color = {{0.0f, 0.0f, 0.0f, 0.0f}}});
        color_render_pass.with_color_attachment(VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            reveal_image_.view,
            VkClearValue{.color = {{1.0f, 0.0f, 0.0f, 0.0f}}});
        color_render_pass.with_depth_attachment(VK_ATTACHMENT_LOAD_OP_LOAD,
            VK_ATTACHMENT_STORE_OP_NONE,
            depth_buffer.view);

        [[maybe_unused]] auto guard{color_render_pass.begin(command_buffer,
            {{0, 0}, accumulation_image_.extent})};

        auto const switch_pipeline =
            []([[maybe_unused]] vkgltf::alpha_mode_t const mode,
                [[maybe_unused]] bool const double_sided) { };

        vkrndr::bind_pipeline(command_buffer, pbr_pipeline_);

        graph.traverse(vkgltf::alpha_mode_t::blend,
            command_buffer,
            *pbr_pipeline_.layout,
            switch_pipeline);
    }

    vkrndr::transition_image(accumulation_image_.image,
        command_buffer,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
        1);

    vkrndr::transition_image(reveal_image_.image,
        command_buffer,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
        1);
    {
        [[maybe_unused]] vkrndr::command_buffer_scope_t const
            composition_pass_scope{command_buffer, "Composition"};

        vkrndr::render_pass_t color_render_pass;
        color_render_pass.with_color_attachment(VK_ATTACHMENT_LOAD_OP_LOAD,
            VK_ATTACHMENT_STORE_OP_STORE,
            color_image.view);

        [[maybe_unused]] auto const guard{
            color_render_pass.begin(command_buffer,
                {{0, 0}, color_image.extent})};

        vkrndr::bind_pipeline(command_buffer,
            composition_pipeline_,
            0,
            std::span{&descriptor_set_, 1});

        vkCmdDraw(command_buffer, 3, 1, 0, 0);
    }
}

void gltfviewer::weighted_oit_shader_t::resize(uint32_t const width,
    uint32_t const height)
{
    destroy(&backend_->device(), &accumulation_image_);
    accumulation_image_ = create_image_and_view(backend_->device(),
        vkrndr::to_extent(width, height),
        1,
        backend_->device().max_msaa_samples,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT);

    destroy(&backend_->device(), &reveal_image_);
    reveal_image_ = create_image_and_view(backend_->device(),
        vkrndr::to_extent(width, height),
        1,
        backend_->device().max_msaa_samples,
        VK_FORMAT_R8_UNORM,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT);

    update_descriptor_set(backend_->device(),
        descriptor_set_,
        vkrndr::combined_sampler_descriptor(accumulation_sampler_,
            accumulation_image_),
        vkrndr::combined_sampler_descriptor(reveal_sampler_, reveal_image_));
}

void gltfviewer::weighted_oit_shader_t::load(render_graph_t const& graph,
    VkDescriptorSetLayout environment_layout,
    VkDescriptorSetLayout materials_layout,
    VkFormat depth_buffer_format)
{
    using namespace std::string_view_literals;

    vkglsl::shader_set_t shader_set{true, false};

    auto vertex_shader{add_shader_module_from_path(shader_set,
        backend_->device(),
        VK_SHADER_STAGE_VERTEX_BIT,
        "pbr.vert")};
    assert(vertex_shader);
    [[maybe_unused]] boost::scope::defer_guard const destroy_vtx{
        [this, shd = &vertex_shader.value()]()
        { destroy(&backend_->device(), shd); }};

    auto fragment_shader{add_shader_module_from_path(shader_set,
        backend_->device(),
        VK_SHADER_STAGE_FRAGMENT_BIT,
        "pbr.frag",
        std::array{"OIT"sv})};
    assert(fragment_shader);
    [[maybe_unused]] boost::scope::defer_guard const destroy_frag{
        [this, shd = &fragment_shader.value()]()
        { destroy(&backend_->device(), shd); }};

    if (composition_pipeline_.pipeline != VK_NULL_HANDLE)
    {
        destroy(&backend_->device(), &pbr_pipeline_);
    }

    VkPipelineColorBlendAttachmentState accumulation_color_blending{};
    accumulation_color_blending.blendEnable = VK_TRUE;
    accumulation_color_blending.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;
    accumulation_color_blending.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    accumulation_color_blending.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    accumulation_color_blending.colorBlendOp = VK_BLEND_OP_ADD;
    accumulation_color_blending.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    accumulation_color_blending.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    accumulation_color_blending.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendAttachmentState reveal_color_blending{};
    reveal_color_blending.blendEnable = VK_TRUE;
    reveal_color_blending.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
    reveal_color_blending.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    reveal_color_blending.dstColorBlendFactor =
        VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    reveal_color_blending.colorBlendOp = VK_BLEND_OP_ADD;
    reveal_color_blending.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    reveal_color_blending.dstAlphaBlendFactor =
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    reveal_color_blending.alphaBlendOp = VK_BLEND_OP_ADD;

    pbr_pipeline_ =
        vkrndr::pipeline_builder_t{backend_->device(),
            vkrndr::pipeline_layout_builder_t{backend_->device()}
                .add_descriptor_set_layout(environment_layout)
                .add_descriptor_set_layout(materials_layout)
                .add_descriptor_set_layout(graph.descriptor_layout())
                .add_push_constants(VkPushConstantRange{
                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT |
                        VK_SHADER_STAGE_FRAGMENT_BIT,
                    .offset = 0,
                    .size = 16})
                .build()}
            .add_shader(as_pipeline_shader(*vertex_shader))
            .add_shader(as_pipeline_shader(*fragment_shader))
            .add_color_attachment(VK_FORMAT_R16G16B16A16_SFLOAT,
                accumulation_color_blending)
            .add_color_attachment(VK_FORMAT_R8_UNORM, reveal_color_blending)
            .with_primitive_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .with_rasterization_samples(backend_->device().max_msaa_samples)
            .with_depth_test(depth_buffer_format, VK_COMPARE_OP_LESS, false)
            .add_vertex_input(graph.binding_description(),
                graph.attribute_description())
            .build();
    object_name(backend_->device(), pbr_pipeline_, "Transparent Pipeline");

    if (composition_pipeline_.pipeline != VK_NULL_HANDLE)
    {
        destroy(&backend_->device(), &composition_pipeline_);
    }

    vkglsl::shader_set_t composition_shader_set{true, false};
    auto composition_vertex_shader{
        vkglsl::add_shader_binary_from_path(composition_shader_set,
            backend_->device(),
            VK_SHADER_STAGE_VERTEX_BIT,
            "fullscreen.vert.spv")};
    assert(composition_vertex_shader);
    [[maybe_unused]] boost::scope::defer_guard const destroy_cvtx{
        [this, shd = &composition_vertex_shader.value()]
        { destroy(&backend_->device(), shd); }};

    auto composition_fragment_shader{
        vkglsl::add_shader_binary_from_path(composition_shader_set,
            backend_->device(),
            VK_SHADER_STAGE_FRAGMENT_BIT,
            "oit_composition.frag.spv")};
    assert(composition_fragment_shader);
    [[maybe_unused]] boost::scope::defer_guard const destroy_cfrag{
        [this, shd = &composition_fragment_shader.value()]
        { destroy(&backend_->device(), shd); }};

    auto const sample_count{cppext::narrow<uint32_t>(
        std::to_underlying(backend_->device().max_msaa_samples))};
    VkSpecializationMapEntry const sample_specialization{.constantID = 0,
        .offset = 0,
        .size = sizeof(sample_count)};
    VkSpecializationInfo const fragment_specialization{.mapEntryCount = 1,
        .pMapEntries = &sample_specialization,
        .dataSize = sizeof(sample_specialization),
        .pData = &sample_count};

    VkPipelineColorBlendAttachmentState composition_color_blending{};
    composition_color_blending.blendEnable = VK_TRUE;
    composition_color_blending.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;
    composition_color_blending.srcColorBlendFactor =
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    composition_color_blending.dstColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    composition_color_blending.colorBlendOp = VK_BLEND_OP_ADD;
    composition_color_blending.srcAlphaBlendFactor =
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    composition_color_blending.dstAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    composition_color_blending.alphaBlendOp = VK_BLEND_OP_ADD;

    composition_pipeline_ =
        vkrndr::pipeline_builder_t{backend_->device(),
            vkrndr::pipeline_layout_builder_t{backend_->device()}
                .add_descriptor_set_layout(descriptor_set_layout_)
                .build()}
            .add_shader(as_pipeline_shader(*composition_vertex_shader))
            .add_shader(as_pipeline_shader(*composition_fragment_shader,
                &fragment_specialization))
            .add_color_attachment(VK_FORMAT_R16G16B16A16_SFLOAT,
                composition_color_blending)
            .with_depth_test(depth_buffer_format, VK_COMPARE_OP_LESS, false)
            .with_rasterization_samples(backend_->device().max_msaa_samples)
            .with_culling(VK_CULL_MODE_FRONT_BIT,
                VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .build();
    object_name(backend_->device(),
        composition_pipeline_,
        "Composition pipeline");
}
