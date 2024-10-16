#include <skybox.hpp>

#include <cppext_numeric.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_buffer.hpp>
#include <vkrndr_commands.hpp>
#include <vkrndr_cubemap.hpp>
#include <vkrndr_descriptors.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_memory.hpp>
#include <vkrndr_pipeline.hpp>
#include <vkrndr_render_pass.hpp>
#include <vkrndr_shader_module.hpp>
#include <vkrndr_transient_operation.hpp>
#include <vkrndr_utility.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>

#include <stb_image.h>

#include <volk.h>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>

// IWYU pragma: no_include <glm/detail/qualifier.hpp>
// IWYU pragma: no_include <memory>

namespace
{
    struct [[nodiscard]] cubemap_push_constants_t final
    {
        uint32_t direction;
    };

    struct [[nodiscard]] cubemap_uniform_t final
    {
        glm::mat4 projection;
        glm::mat4 directions[6];
    };

    [[nodiscard]] VkSampler create_cubemap_sampler(
        vkrndr::device_t const& device)
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

    [[nodiscard]] VkDescriptorSetLayout create_cubemap_descriptor_set_layout(
        vkrndr::device_t const& device)
    {
        VkDescriptorSetLayoutBinding cubemap_binding{};
        cubemap_binding.binding = 0;
        cubemap_binding.descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        cubemap_binding.descriptorCount = 1;
        cubemap_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding projection_binding{};
        projection_binding.binding = 1;
        projection_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        projection_binding.descriptorCount = 1;
        projection_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        std::array bindings{cubemap_binding, projection_binding};

        return vkrndr::create_descriptor_set_layout(device, bindings);
    }

    void update_cubemap_descriptor(vkrndr::device_t const& device,
        VkDescriptorSet const descriptor_set,
        VkDescriptorImageInfo const cubemap_sampler,
        VkDescriptorBufferInfo const uniform_buffer)
    {
        VkWriteDescriptorSet sampler_write{};
        sampler_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        sampler_write.dstSet = descriptor_set;
        sampler_write.dstBinding = 0;
        sampler_write.dstArrayElement = 0;
        sampler_write.descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        sampler_write.descriptorCount = 1;
        sampler_write.pImageInfo = &cubemap_sampler;

        VkWriteDescriptorSet uniform_write{};
        uniform_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        uniform_write.dstSet = descriptor_set;
        uniform_write.dstBinding = 1;
        uniform_write.dstArrayElement = 0;
        uniform_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uniform_write.descriptorCount = 1;
        uniform_write.pBufferInfo = &uniform_buffer;

        std::array const descriptor_writes{sampler_write, uniform_write};

        vkUpdateDescriptorSets(device.logical,
            vkrndr::count_cast(descriptor_writes.size()),
            descriptor_writes.data(),
            0,
            nullptr);
    }

    consteval auto binding_description()
    {
        constexpr std::array descriptions{
            VkVertexInputBindingDescription{.binding = 0,
                .stride = sizeof(glm::vec3),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX},
        };

        return descriptions;
    }

    consteval auto attribute_descriptions()
    {
        constexpr std::array descriptions{
            VkVertexInputAttributeDescription{.location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = 0}};

        return descriptions;
    }

    [[nodiscard]] VkSampler create_skybox_sampler(
        vkrndr::device_t const& device,
        uint32_t const mip_levels)
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
        sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        sampler_info.unnormalizedCoordinates = VK_FALSE;
        sampler_info.compareEnable = VK_FALSE;
        sampler_info.compareOp = VK_COMPARE_OP_NEVER;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler_info.mipLodBias = 0.0f;
        sampler_info.minLod = 0.0f;
        sampler_info.maxLod = cppext::as_fp(mip_levels);

        VkSampler rv; // NOLINT
        vkrndr::check_result(
            vkCreateSampler(device.logical, &sampler_info, nullptr, &rv));

        return rv;
    }

    [[nodiscard]] VkDescriptorSetLayout create_skybox_descriptor_set_layout(
        vkrndr::device_t const& device)
    {
        VkDescriptorSetLayoutBinding cubemap_binding{};
        cubemap_binding.binding = 0;
        cubemap_binding.descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        cubemap_binding.descriptorCount = 1;
        cubemap_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        return vkrndr::create_descriptor_set_layout(device,
            std::span{&cubemap_binding, 1});
    }

    void update_skybox_descriptor(vkrndr::device_t const& device,
        VkDescriptorSet const descriptor_set,
        VkDescriptorImageInfo const cubemap_sampler)
    {
        VkWriteDescriptorSet sampler_write{};
        sampler_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        sampler_write.dstSet = descriptor_set;
        sampler_write.dstBinding = 0;
        sampler_write.dstArrayElement = 0;
        sampler_write.descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        sampler_write.descriptorCount = 1;
        sampler_write.pImageInfo = &cubemap_sampler;

        vkUpdateDescriptorSets(device.logical, 1, &sampler_write, 0, nullptr);
    }
} // namespace

gltfviewer::skybox_t::skybox_t(vkrndr::backend_t& backend) : backend_{&backend}
{
}

gltfviewer::skybox_t::~skybox_t()
{
    destroy(&backend_->device(), &irradiance_cubemap_);

    destroy(&backend_->device(), &skybox_pipeline_);

    vkrndr::free_descriptor_sets(backend_->device(),
        backend_->descriptor_pool(),
        std::span{&skybox_descriptor_, 1});

    vkDestroyDescriptorSetLayout(backend_->device().logical,
        skybox_descriptor_layout_,
        nullptr);

    vkDestroySampler(backend_->device().logical, skybox_sampler_, nullptr);

    destroy(&backend_->device(), &cubemap_uniform_buffer_);

    destroy(&backend_->device(), &cubemap_index_buffer_);

    destroy(&backend_->device(), &cubemap_vertex_buffer_);

    destroy(&backend_->device(), &cubemap_);
}

void gltfviewer::skybox_t::load_hdr(std::filesystem::path const& hdr_image,
    VkDescriptorSetLayout environment_layout,
    VkFormat depth_buffer_format)
{
    int width; // NOLINT
    int height; // NOLINT
    int components; // NOLINT
    stbi_set_flip_vertically_on_load(1);
    float* const hdr_texture_data{stbi_loadf(hdr_image.string().c_str(),
        &width,
        &height,
        &components,
        4)};
    stbi_set_flip_vertically_on_load(0);
    assert(hdr_texture_data);

    auto const hdr_extent{vkrndr::to_extent(width, height)};
    vkrndr::image_t cubemap_texture = backend_->transfer_image(
        vkrndr::as_bytes(std::span{hdr_texture_data,
            size_t{hdr_extent.width} * hdr_extent.height * 4}),
        hdr_extent,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        1);
    stbi_image_free(hdr_texture_data);

    cubemap_ = vkrndr::create_cubemap(backend_->device(),
        1024,
        vkrndr::max_mip_levels(1024, 1024),
        VK_SAMPLE_COUNT_1_BIT,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    {
        auto staging_buffer{vkrndr::create_staging_buffer(backend_->device(),
            sizeof(glm::vec3) * 8)};
        auto map{vkrndr::map_memory(backend_->device(), staging_buffer)};

        glm::vec3* const vertices{map.as<glm::vec3>()};
        vertices[0] = glm::vec3{-1, -1, -1};
        vertices[1] = glm::vec3{1, -1, -1};
        vertices[2] = glm::vec3{1, 1, -1};
        vertices[3] = glm::vec3{-1, 1, -1};
        vertices[4] = glm::vec3{-1, -1, 1};
        vertices[5] = glm::vec3{1, -1, 1};
        vertices[6] = glm::vec3{1, 1, 1};
        vertices[7] = glm::vec3{-1, 1, 1};

        unmap_memory(backend_->device(), &map);

        cubemap_vertex_buffer_ = vkrndr::create_buffer(backend_->device(),
            staging_buffer.size,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        backend_->transfer_buffer(staging_buffer, cubemap_vertex_buffer_);

        destroy(&backend_->device(), &staging_buffer);
    }

    {
        auto staging_buffer{vkrndr::create_staging_buffer(backend_->device(),
            sizeof(uint32_t) * 36)};
        auto map{vkrndr::map_memory(backend_->device(), staging_buffer)};

        uint32_t* const indices{map.as<uint32_t>()};

        indices[0] = 0;
        indices[1] = 1;
        indices[2] = 2;
        indices[3] = 0;
        indices[4] = 2;
        indices[5] = 3;

        indices[6] = 4;
        indices[7] = 6;
        indices[8] = 5;
        indices[9] = 4;
        indices[10] = 7;
        indices[11] = 6;

        indices[12] = 0;
        indices[13] = 3;
        indices[14] = 7;
        indices[15] = 0;
        indices[16] = 7;
        indices[17] = 4;

        indices[18] = 1;
        indices[19] = 5;
        indices[20] = 6;
        indices[21] = 1;
        indices[22] = 6;
        indices[23] = 2;

        indices[24] = 3;
        indices[25] = 2;
        indices[26] = 6;
        indices[27] = 3;
        indices[28] = 6;
        indices[29] = 7;

        indices[30] = 0;
        indices[31] = 4;
        indices[32] = 5;
        indices[33] = 0;
        indices[34] = 5;
        indices[35] = 1;

        unmap_memory(backend_->device(), &map);

        cubemap_index_buffer_ = vkrndr::create_buffer(backend_->device(),
            staging_buffer.size,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        backend_->transfer_buffer(staging_buffer, cubemap_index_buffer_);

        destroy(&backend_->device(), &staging_buffer);
    }

    {
        auto staging_buffer{vkrndr::create_staging_buffer(backend_->device(),
            sizeof(cubemap_uniform_t))};
        auto map{map_memory(backend_->device(), staging_buffer)};
        auto* const uniform{map.as<cubemap_uniform_t>()};

        uniform->projection =
            glm::perspectiveRH_ZO(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
        uniform->directions[0] = glm::lookAtRH(glm::vec3{0.0f, 0.0f, 0.0f},
            glm::vec3{1.0f, 0.0f, 0.0f},
            glm::vec3{0.0f, -1.0f, 0.0f});
        uniform->directions[1] = glm::lookAtRH(glm::vec3{0.0f, 0.0f, 0.0f},
            glm::vec3{-1.0f, 0.0f, 0.0f},
            glm::vec3{0.0f, -1.0f, 0.0f});
        uniform->directions[2] = glm::lookAtRH(glm::vec3{0.0f, 0.0f, 0.0f},
            glm::vec3{0.0f, 1.0f, 0.0f},
            glm::vec3{0.0f, 0.0f, 1.0f});
        uniform->directions[3] = glm::lookAtRH(glm::vec3{0.0f, 0.0f, 0.0f},
            glm::vec3{0.0f, -1.0f, 0.0f},
            glm::vec3{0.0f, 0.0f, -1.0f});
        uniform->directions[4] = glm::lookAtRH(glm::vec3{0.0f, 0.0f, 0.0f},
            glm::vec3{0.0f, 0.0f, 1.0f},
            glm::vec3{0.0f, -1.0f, 0.0f});
        uniform->directions[5] = glm::lookAtRH(glm::vec3{0.0f, 0.0f, 0.0f},
            glm::vec3{0.0f, 0.0f, -1.0f},
            glm::vec3{0.0f, -1.0f, 0.0f});

        unmap_memory(backend_->device(), &map);

        cubemap_uniform_buffer_ = vkrndr::create_buffer(backend_->device(),
            staging_buffer.size,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        backend_->transfer_buffer(staging_buffer, cubemap_uniform_buffer_);

        destroy(&backend_->device(), &staging_buffer);
    }

    VkSampler const cubemap_sampler{create_cubemap_sampler(backend_->device())};

    VkDescriptorSetLayout const cubemap_descriptor_layout{
        create_cubemap_descriptor_set_layout(backend_->device())};

    VkDescriptorSet cubemap_descriptor; // NOLINT
    vkrndr::create_descriptor_sets(backend_->device(),
        backend_->descriptor_pool(),
        std::span{&cubemap_descriptor_layout, 1},
        std::span{&cubemap_descriptor, 1});

    update_cubemap_descriptor(backend_->device(),
        cubemap_descriptor,
        vkrndr::combined_sampler_descriptor(cubemap_sampler, cubemap_texture),
        vkrndr::buffer_descriptor(cubemap_uniform_buffer_));

    generate_cubemap_faces(cubemap_descriptor_layout, cubemap_descriptor);

    skybox_sampler_ =
        create_skybox_sampler(backend_->device(), cubemap_.mip_levels);

    skybox_descriptor_layout_ =
        create_skybox_descriptor_set_layout(backend_->device());

    vkrndr::create_descriptor_sets(backend_->device(),
        backend_->descriptor_pool(),
        std::span{&skybox_descriptor_layout_, 1},
        std::span{&skybox_descriptor_, 1});

    update_skybox_descriptor(backend_->device(),
        skybox_descriptor_,
        vkrndr::combined_sampler_descriptor(skybox_sampler_, cubemap_));

    auto skybox_vertex_shader{vkrndr::create_shader_module(backend_->device(),
        "skybox.vert.spv",
        VK_SHADER_STAGE_VERTEX_BIT,
        "main")};

    auto skybox_fragment_shader{vkrndr::create_shader_module(backend_->device(),
        "skybox.frag.spv",
        VK_SHADER_STAGE_FRAGMENT_BIT,
        "main")};

    skybox_pipeline_ =
        vkrndr::pipeline_builder_t{backend_->device(),
            vkrndr::pipeline_layout_builder_t{backend_->device()}
                .add_descriptor_set_layout(environment_layout)
                .add_descriptor_set_layout(skybox_descriptor_layout_)
                .build(),
            VK_FORMAT_R16G16B16A16_SFLOAT}
            .add_shader(as_pipeline_shader(skybox_vertex_shader))
            .add_shader(as_pipeline_shader(skybox_fragment_shader))
            .with_primitive_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .with_rasterization_samples(backend_->device().max_msaa_samples)
            .with_depth_test(depth_buffer_format, VK_COMPARE_OP_LESS_OR_EQUAL)
            .add_vertex_input(binding_description(), attribute_descriptions())
            .build();

    destroy(&backend_->device(), &skybox_vertex_shader);
    destroy(&backend_->device(), &skybox_fragment_shader);

    irradiance_cubemap_ = vkrndr::create_cubemap(backend_->device(),
        32,
        vkrndr::max_mip_levels(32, 32),
        VK_SAMPLE_COUNT_1_BIT,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    generate_irradiance_map(cubemap_descriptor_layout, cubemap_descriptor);

    vkrndr::free_descriptor_sets(backend_->device(),
        backend_->descriptor_pool(),
        std::span{&cubemap_descriptor, 1});

    vkDestroyDescriptorSetLayout(backend_->device().logical,
        cubemap_descriptor_layout,
        nullptr);

    vkDestroySampler(backend_->device().logical, cubemap_sampler, nullptr);

    destroy(&backend_->device(), &cubemap_texture);
}

void gltfviewer::skybox_t::draw(VkCommandBuffer command_buffer,
    vkrndr::image_t const& color_image,
    vkrndr::image_t const& depth_buffer)
{
    VkDeviceSize const zero_offset{};
    vkCmdBindVertexBuffers(command_buffer,
        0,
        1,
        &cubemap_vertex_buffer_.buffer,
        &zero_offset);

    vkCmdBindIndexBuffer(command_buffer,
        cubemap_index_buffer_.buffer,
        0,
        VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        *skybox_pipeline_.layout,
        1,
        1,
        &skybox_descriptor_,
        0,
        nullptr);

    vkrndr::render_pass_t color_render_pass;
    color_render_pass.with_color_attachment(VK_ATTACHMENT_LOAD_OP_LOAD,
        VK_ATTACHMENT_STORE_OP_STORE,
        color_image.view);
    color_render_pass.with_depth_attachment(VK_ATTACHMENT_LOAD_OP_LOAD,
        VK_ATTACHMENT_STORE_OP_NONE,
        depth_buffer.view);

    [[maybe_unused]] auto guard{
        color_render_pass.begin(command_buffer, {{0, 0}, color_image.extent})};

    vkrndr::bind_pipeline(command_buffer,
        skybox_pipeline_,
        1,
        std::span{&skybox_descriptor_, 1});

    vkCmdDrawIndexed(command_buffer, 36, 1, 0, 0, 0);
}

VkPipelineLayout gltfviewer::skybox_t::pipeline_layout() const
{
    return *skybox_pipeline_.layout;
}

void gltfviewer::skybox_t::generate_cubemap_faces(VkDescriptorSetLayout layout,
    VkDescriptorSet descriptor_set)
{
    auto cubemap_vertex_shader{vkrndr::create_shader_module(backend_->device(),
        "cubemap.vert.spv",
        VK_SHADER_STAGE_VERTEX_BIT,
        "main")};

    auto cubemap_fragment_shader{
        vkrndr::create_shader_module(backend_->device(),
            "equirectangular_to_cubemap.frag.spv",
            VK_SHADER_STAGE_FRAGMENT_BIT,
            "main")};

    auto pipeline =
        vkrndr::pipeline_builder_t{backend_->device(),
            vkrndr::pipeline_layout_builder_t{backend_->device()}
                .add_descriptor_set_layout(layout)
                .add_push_constants<cubemap_push_constants_t>(
                    VK_SHADER_STAGE_VERTEX_BIT)
                .build(),
            cubemap_.format}
            .add_shader(as_pipeline_shader(cubemap_vertex_shader))
            .add_shader(as_pipeline_shader(cubemap_fragment_shader))
            .with_primitive_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .with_rasterization_samples(VK_SAMPLE_COUNT_1_BIT)
            .add_vertex_input(binding_description(), attribute_descriptions())
            .build();

    destroy(&backend_->device(), &cubemap_vertex_shader);
    destroy(&backend_->device(), &cubemap_fragment_shader);

    render_to_cubemap(pipeline, std::span{&descriptor_set, 1}, cubemap_);

    destroy(&backend_->device(), &pipeline);
}

void gltfviewer::skybox_t::generate_irradiance_map(VkDescriptorSetLayout layout,
    VkDescriptorSet descriptor_set)
{
    auto irradiance_vertex_shader{
        vkrndr::create_shader_module(backend_->device(),
            "cubemap.vert.spv",
            VK_SHADER_STAGE_VERTEX_BIT,
            "main")};

    auto irradiance_fragment_shader{
        vkrndr::create_shader_module(backend_->device(),
            "irradiance.frag.spv",
            VK_SHADER_STAGE_FRAGMENT_BIT,
            "main")};

    auto pipeline =
        vkrndr::pipeline_builder_t{backend_->device(),
            vkrndr::pipeline_layout_builder_t{backend_->device()}
                .add_descriptor_set_layout(layout)
                .add_descriptor_set_layout(skybox_descriptor_layout_)
                .add_push_constants<cubemap_push_constants_t>(
                    VK_SHADER_STAGE_VERTEX_BIT)
                .build(),
            irradiance_cubemap_.format}
            .add_shader(as_pipeline_shader(irradiance_vertex_shader))
            .add_shader(as_pipeline_shader(irradiance_fragment_shader))
            .with_primitive_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .with_rasterization_samples(VK_SAMPLE_COUNT_1_BIT)
            .add_vertex_input(binding_description(), attribute_descriptions())
            .build();

    destroy(&backend_->device(), &irradiance_vertex_shader);
    destroy(&backend_->device(), &irradiance_fragment_shader);

    std::array descriptors{descriptor_set, skybox_descriptor_};

    render_to_cubemap(pipeline, descriptors, irradiance_cubemap_);

    destroy(&backend_->device(), &pipeline);
}

void gltfviewer::skybox_t::render_to_cubemap(vkrndr::pipeline_t const& pipeline,
    std::span<VkDescriptorSet const> const& descriptors,
    vkrndr::cubemap_t& cubemap)
{
    auto transient{backend_->request_transient_operation(false)};
    VkCommandBuffer command_buffer{transient.command_buffer()};

    VkViewport const viewport{.x = 0.0f,
        .y = 0.0f,
        .width = cppext::as_fp(cubemap.extent.width),
        .height = cppext::as_fp(cubemap.extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f};
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D const scissor{{0, 0}, cubemap.extent};
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    VkDeviceSize const zero_offset{};
    vkCmdBindVertexBuffers(command_buffer,
        0,
        1,
        &cubemap_vertex_buffer_.buffer,
        &zero_offset);

    vkCmdBindIndexBuffer(command_buffer,
        cubemap_index_buffer_.buffer,
        0,
        VK_INDEX_TYPE_UINT32);

    vkrndr::bind_pipeline(command_buffer, pipeline, 0, descriptors);

    vkrndr::transition_image(cubemap.image,
        command_buffer,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        cubemap.mip_levels,
        6);

    for (uint32_t i{}; i != cubemap.face_views.size(); ++i)
    {
        cubemap_push_constants_t pc{.direction = i};
        vkCmdPushConstants(command_buffer,
            *pipeline.layout,
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(cubemap_push_constants_t),
            &pc);

        vkrndr::render_pass_t color_render_pass;
        color_render_pass.with_color_attachment(VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            cubemap.face_views[i]);

        [[maybe_unused]] auto guard{
            color_render_pass.begin(command_buffer, {{0, 0}, cubemap.extent})};

        vkCmdDrawIndexed(command_buffer, 36, 1, 0, 0, 0);
    }

    vkrndr::transition_image(cubemap.image,
        command_buffer,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_2_BLIT_BIT,
        VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT,
        cubemap.mip_levels,
        6);

    generate_mipmaps(backend_->device(),
        cubemap.image,
        command_buffer,
        cubemap.format,
        cubemap.extent,
        cubemap.mip_levels,
        6);
}
