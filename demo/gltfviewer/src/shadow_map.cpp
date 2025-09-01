#include <shadow_map.hpp>

#include <config.hpp>
#include <scene_graph.hpp>

#include <cppext_container.hpp>

#include <ngnast_scene_model.hpp>

#include <vkglsl_shader_set.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_debug_utils.hpp>
#include <vkrndr_descriptors.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_formats.hpp>
#include <vkrndr_graphics_pipeline_builder.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_pipeline.hpp>
#include <vkrndr_pipeline_layout_builder.hpp>
#include <vkrndr_render_pass.hpp>
#include <vkrndr_shader_module.hpp>
#include <vkrndr_synchronization.hpp>
#include <vkrndr_transient_operation.hpp>
#include <vkrndr_utility.hpp>

#include <volk.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <exception>
#include <filesystem>
#include <functional>
#include <iterator>
#include <vector>

// IWYU pragma: no_include <expected>
// IWYU pragma: no_include <chrono>
// IWYU pragma: no_include <memory>
// IWYU pragma: no_include <optional>
// IWYU pragma: no_include <type_traits>
// IWYU pragma: no_include <span>
// IWYU pragma: no_include <string_view>
// IWYU pragma: no_include <system_error>
// IWYU pragma: no_forward_declare VkDescriptorSet_T

namespace
{
    [[nodiscard]] vkrndr::image_t create_depth_buffer(
        vkrndr::backend_t const& backend)
    {
        auto const& formats{
            vkrndr::find_supported_depth_stencil_formats(backend.device(),
                true,
                false)};
        vkrndr::image_2d_create_info_t create_info{.extent = {1024, 1024},
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                VK_IMAGE_USAGE_SAMPLED_BIT,
            .required_memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT};

        auto const opt_it{std::ranges::find_if(formats,
            [](auto const& f)
            {
                return f.properties.optimalTilingFeatures &
                    VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
            })};
        if (opt_it != std::cend(formats))
        {
            create_info.format = opt_it->format;
            create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
            return create_image_and_view(backend.device(),
                create_info,
                VK_IMAGE_ASPECT_DEPTH_BIT);
        }

        auto const lin_it{std::ranges::find_if(formats,
            [](auto const& f)
            {
                return f.properties.linearTilingFeatures &
                    VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
            })};
        if (lin_it != std::cend(formats))
        {
            create_info.format = lin_it->format;
            create_info.tiling = VK_IMAGE_TILING_LINEAR;
            return create_image_and_view(backend.device(),
                create_info,
                VK_IMAGE_ASPECT_DEPTH_BIT);
        }

        std::terminate();
    }

    [[nodiscard]] VkDescriptorSetLayout create_descriptor_set_layout(
        vkrndr::device_t const& device)
    {
        VkDescriptorSetLayoutBinding sampler_binding{};
        sampler_binding.binding = 0;
        sampler_binding.descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        sampler_binding.descriptorCount = 1;
        sampler_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        return vkrndr::create_descriptor_set_layout(device,
            cppext::as_span(sampler_binding))
            .value();
    }

    void update_descriptor_set(vkrndr::device_t const& device,
        VkDescriptorSet const descriptor_set,
        VkDescriptorImageInfo const shadow_map_info)
    {
        VkWriteDescriptorSet shadow_map_write{};
        shadow_map_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        shadow_map_write.dstSet = descriptor_set;
        shadow_map_write.dstBinding = 0;
        shadow_map_write.dstArrayElement = 0;
        shadow_map_write.descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        shadow_map_write.descriptorCount = 1;
        shadow_map_write.pImageInfo = &shadow_map_info;

        vkUpdateDescriptorSets(device, 1, &shadow_map_write, 0, nullptr);
    }

    [[nodiscard]] VkSampler create_shadow_map_sampler(
        vkrndr::device_t const& device)
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
        sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
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

gltfviewer::shadow_map_t::shadow_map_t(vkrndr::backend_t& backend)
    : backend_{&backend}
    , shadow_map_{create_depth_buffer(*backend_)}
    , shadow_sampler_{create_shadow_map_sampler(backend_->device())}
    , descriptor_layout_{create_descriptor_set_layout(backend_->device())}
{
    vkrndr::check_result(allocate_descriptor_sets(backend_->device(),
        backend_->descriptor_pool(),
        cppext::as_span(descriptor_layout_),
        cppext::as_span(descriptor_)));

    {
        vkrndr::transient_operation_t const transient{
            backend_->request_transient_operation(false)};

        auto const barrier{vkrndr::to_layout(
            vkrndr::image_barrier(shadow_map_, VK_IMAGE_ASPECT_DEPTH_BIT),
            VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL)};

        vkrndr::wait_for(transient.command_buffer(),
            {},
            {},
            cppext::as_span(barrier));
    }

    ::update_descriptor_set(backend_->device(),
        descriptor_,
        vkrndr::combined_sampler_descriptor(shadow_sampler_,
            shadow_map_,
            VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL));
}

gltfviewer::shadow_map_t::~shadow_map_t()
{
    destroy(&backend_->device(), &depth_pipeline_);

    free_descriptor_sets(backend_->device(),
        backend_->descriptor_pool(),
        cppext::as_span(descriptor_));

    vkDestroyDescriptorSetLayout(backend_->device(),
        descriptor_layout_,
        nullptr);

    vkDestroySampler(backend_->device(), shadow_sampler_, nullptr);

    destroy(backend_->device(), shadow_map_);
    destroy(backend_->device(), vertex_shader_);
}

VkPipelineLayout gltfviewer::shadow_map_t::pipeline_layout() const
{
    if (depth_pipeline_.pipeline)
    {
        return *depth_pipeline_.layout;
    }

    return VK_NULL_HANDLE;
}

VkDescriptorSetLayout gltfviewer::shadow_map_t::descriptor_layout() const
{
    return descriptor_layout_;
}

void gltfviewer::shadow_map_t::draw(scene_graph_t const& graph,
    VkCommandBuffer command_buffer) const
{
    vkrndr::render_pass_t shadow_map_pass;
    shadow_map_pass.with_depth_attachment(VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        shadow_map_.view,
        VkClearValue{.depthStencil = {1.0f, 0}});

    [[maybe_unused]] auto guard{shadow_map_pass.begin(command_buffer,
        {{0, 0}, vkrndr::to_2d_extent(shadow_map_.extent)})};

    VKRNDR_IF_DEBUG_UTILS(
        [[maybe_unused]] vkrndr::command_buffer_scope_t const depth_pass_scope{
            command_buffer,
            "Shadow Map"});

    vkrndr::bind_pipeline(command_buffer, depth_pipeline_);

    graph.traverse(ngnast::alpha_mode_t::opaque,
        command_buffer,
        *depth_pipeline_.layout,
        []([[maybe_unused]] ngnast::alpha_mode_t const mode,
            [[maybe_unused]] bool const double_sided) { });

    auto const barrier{vkrndr::with_layout(
        vkrndr::with_access(vkrndr::on_stage(vkrndr::image_barrier(shadow_map_,
                                                 VK_IMAGE_ASPECT_DEPTH_BIT),
                                VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT),
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT),
        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL)};

    vkrndr::wait_for(command_buffer, {}, {}, cppext::as_span(barrier));
}

void gltfviewer::shadow_map_t::bind_on(VkCommandBuffer command_buffer,
    VkPipelineLayout layout,
    VkPipelineBindPoint bind_point)
{
    vkCmdBindDescriptorSets(command_buffer,
        bind_point,
        layout,
        3,
        1,
        &descriptor_,
        0,
        nullptr);
}

void gltfviewer::shadow_map_t::load(scene_graph_t const& graph,
    VkDescriptorSetLayout environment_layout,
    VkDescriptorSetLayout materials_layout,
    VkFormat depth_buffer_format)
{
    using namespace std::string_view_literals;

    vkglsl::shader_set_t shader_set{enable_shader_debug_symbols,
        enable_shader_optimization};

    std::filesystem::path const vertex_path{"pbr.vert"};
    if (auto const wt{last_write_time(vertex_path)}; vertex_write_time_ != wt)
    {
        if (vertex_shader_.handle)
        {
            destroy(backend_->device(), vertex_shader_);
        }

        auto vertex_shader{add_shader_module_from_path(shader_set,
            backend_->device(),
            VK_SHADER_STAGE_VERTEX_BIT,
            vertex_path,
            std::array{"DEPTH_PASS"sv, "SHADOW_PASS"sv})};
        assert(vertex_shader);

        vertex_shader_ = *vertex_shader;

        VKRNDR_IF_DEBUG_UTILS(
            object_name(backend_->device(), vertex_shader_, "PBR vertex"));
        vertex_write_time_ = wt;
    }

    if (depth_pipeline_.pipeline != VK_NULL_HANDLE)
    {
        destroy(&backend_->device(), &depth_pipeline_);
    }

    depth_pipeline_ = vkrndr::graphics_pipeline_builder_t{backend_->device(),
        vkrndr::pipeline_layout_builder_t{backend_->device()}
            .add_descriptor_set_layout(environment_layout)
            .add_descriptor_set_layout(materials_layout)
            .add_descriptor_set_layout(graph.descriptor_layout())
            .add_push_constants(
                VkPushConstantRange{.stageFlags = VK_SHADER_STAGE_VERTEX_BIT |
                        VK_SHADER_STAGE_FRAGMENT_BIT,
                    .offset = 0,
                    .size = 16})
            .build()}
                          .add_shader(as_pipeline_shader(vertex_shader_))
                          .with_depth_test(depth_buffer_format)
                          .add_vertex_input(graph.binding_description(),
                              graph.attribute_description())
                          .with_culling(VK_CULL_MODE_FRONT_BIT,
                              VK_FRONT_FACE_COUNTER_CLOCKWISE)
                          .build();
    VKRNDR_IF_DEBUG_UTILS(
        object_name(backend_->device(), depth_pipeline_, "Depth Pipeline"));
}
