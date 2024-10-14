#include <environment.hpp>

#include <cppext_cycled_buffer.hpp>
#include <cppext_numeric.hpp>

#include <niku_camera.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_buffer.hpp>
#include <vkrndr_descriptors.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_memory.hpp>
#include <vkrndr_render_pass.hpp>
#include <vkrndr_shader_module.hpp>
#include <vkrndr_utility.hpp>

#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <imgui.h>

#include <stb_image.h>

#include <volk.h>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>

// IWYU pragma: no_include <glm/detail/qualifier.hpp>

namespace
{
    struct [[nodiscard]] environment_uniform_t final
    {
        glm::mat4 view;
        glm::mat4 projection;
        glm::vec3 camera_position;
        uint32_t light_count;
    };

    static_assert(sizeof(environment_uniform_t) % 16 == 0);

    struct [[nodiscard]] light_t final
    {
        glm::vec4 position;
        glm::vec4 color;
    };

    struct [[nodiscard]] cubemap_push_constants_t final
    {
        uint32_t direction;
    };

    struct [[nodiscard]] cubemap_uniform_t final
    {
        glm::mat4 projection;
        glm::mat4 directions[6];
    };

    [[nodiscard]] VkDescriptorSetLayout create_descriptor_set_layout(
        vkrndr::device_t const& device)
    {
        VkDescriptorSetLayoutBinding uniform_binding{};
        uniform_binding.binding = 0;
        uniform_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        uniform_binding.descriptorCount = 1;
        uniform_binding.stageFlags =
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        return vkrndr::create_descriptor_set_layout(device,
            std::span{&uniform_binding, 1});
    }

    void update_descriptor_set(vkrndr::device_t const& device,
        VkDescriptorSet const descriptor_set,
        VkDescriptorBufferInfo const uniform_info)
    {
        VkWriteDescriptorSet uniform_write{};
        uniform_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        uniform_write.dstSet = descriptor_set;
        uniform_write.dstBinding = 0;
        uniform_write.dstArrayElement = 0;
        uniform_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        uniform_write.descriptorCount = 1;
        uniform_write.pBufferInfo = &uniform_info;

        std::array const descriptor_writes{uniform_write};

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
} // namespace

gltfviewer::environment_t::environment_t(vkrndr::backend_t& backend)
    : backend_{&backend}
    , lights_{11}
    , descriptor_layout_{create_descriptor_set_layout(backend_->device())}
    , frame_data_{backend_->frames_in_flight(), backend_->frames_in_flight()}
{
    float x_coord{-10.0f};
    for (light_t& light : lights_)
    {
        light.position.x = x_coord;
        x_coord += 2.0f;
    }
    lights_[5].enabled = true;

    for (auto& data : frame_data_.as_span())
    {
        auto const uniform_buffer_size{
            sizeof(environment_uniform_t) + sizeof(::light_t) * lights_.size()};

        data.uniform = create_buffer(backend_->device(),
            uniform_buffer_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        data.uniform_map = vkrndr::map_memory(backend_->device(), data.uniform);

        vkrndr::create_descriptor_sets(backend_->device(),
            backend_->descriptor_pool(),
            std::span{&descriptor_layout_, 1},
            std::span{&data.descriptor_set, 1});

        update_descriptor_set(backend_->device(),
            data.descriptor_set,
            vkrndr::buffer_descriptor(data.uniform));
    }
}

gltfviewer::environment_t::~environment_t()
{
    for (auto& data : frame_data_.as_span())
    {
        vkrndr::free_descriptor_sets(backend_->device(),
            backend_->descriptor_pool(),
            std::span{&data.descriptor_set, 1});

        unmap_memory(backend_->device(), &data.uniform_map);
        destroy(&backend_->device(), &data.uniform);
    }

    vkDestroyDescriptorSetLayout(backend_->device().logical,
        descriptor_layout_,
        nullptr);

    destroy(&backend_->device(), &cubemap_pipeline_);

    vkrndr::free_descriptor_sets(backend_->device(),
        backend_->descriptor_pool(),
        std::span{&cubemap_descriptor_, 1});

    vkDestroyDescriptorSetLayout(backend_->device().logical,
        cubemap_descriptor_layout_,
        nullptr);

    vkDestroySampler(backend_->device().logical, cubemap_sampler_, nullptr);

    destroy(&backend_->device(), &cubemap_uniform_buffer_);

    destroy(&backend_->device(), &cubemap_index_buffer_);

    destroy(&backend_->device(), &cubemap_vertex_buffer_);

    destroy(&backend_->device(), &cubemap_);

    destroy(&backend_->device(), &cubemap_texture_);
}

VkDescriptorSetLayout gltfviewer::environment_t::descriptor_layout() const
{
    return descriptor_layout_;
}

void gltfviewer::environment_t::update(niku::camera_t const& camera)
{
    frame_data_.cycle();

    ImGui::Begin("Lights");
    for (size_t i{}; i != lights_.size(); ++i)
    {
        ImGui::PushID(cppext::narrow<int>(i));
        ImGui::Checkbox("Enabled", &lights_[i].enabled);
        ImGui::SliderFloat3("Position",
            glm::value_ptr(lights_[i].position),
            -500.0f,
            500.0f);
        ImGui::SliderFloat3("Color",
            glm::value_ptr(lights_[i].color),
            0.0f,
            1.0f);
        ImGui::PopID();
    }
    ImGui::End();

    auto* const header{frame_data_->uniform_map.as<environment_uniform_t>()};
    header->view = camera.view_matrix();
    header->projection = camera.projection_matrix();
    header->camera_position = camera.position();

    auto* const light_array{
        frame_data_->uniform_map.as<::light_t>(sizeof(environment_uniform_t))};

    uint32_t enabled_lights{};
    for (light_t const& light : lights_)
    {
        if (light.enabled)
        {
            light_array[enabled_lights] = {
                .position = glm::vec4{light.position, 0.0f},
                .color = glm::vec4{light.color, 0.0f},
            };

            ++enabled_lights;
        }
    }
    header->light_count = enabled_lights;
}

void gltfviewer::environment_t::bind_on(VkCommandBuffer command_buffer,
    VkPipelineLayout layout,
    VkPipelineBindPoint bind_point)
{
    vkCmdBindDescriptorSets(command_buffer,
        bind_point,
        layout,
        0,
        1,
        &frame_data_->descriptor_set,
        0,
        nullptr);
}

void gltfviewer::environment_t::load_hdr(std::filesystem::path const& hdr_image)
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
    assert(hdr_texture_data);

    auto const hdr_extent{vkrndr::to_extent(width, height)};
    cubemap_texture_ = backend_->transfer_image(
        vkrndr::as_bytes(std::span{hdr_texture_data,
            size_t{hdr_extent.width} * hdr_extent.height * 4}),
        hdr_extent,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        1);
    stbi_image_free(hdr_texture_data);

    cubemap_ = vkrndr::create_cubemap(backend_->device(),
        1024,
        1,
        VK_SAMPLE_COUNT_1_BIT,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
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
        uniform->directions[0] = glm::lookAt(glm::vec3{0.0f, 0.0f, 0.0f},
            glm::vec3{1.0f, 0.0f, 0.0f},
            glm::vec3{0.0f, -1.0f, 0.0f});
        uniform->directions[1] = glm::lookAt(glm::vec3{0.0f, 0.0f, 0.0f},
            glm::vec3{-1.0f, 0.0f, 0.0f},
            glm::vec3{0.0f, -1.0f, 0.0f});
        uniform->directions[2] = glm::lookAt(glm::vec3{0.0f, 0.0f, 0.0f},
            glm::vec3{0.0f, 1.0f, 0.0f},
            glm::vec3{0.0f, 0.0f, 1.0f});
        uniform->directions[3] = glm::lookAt(glm::vec3{0.0f, 0.0f, 0.0f},
            glm::vec3{0.0f, -1.0f, 0.0f},
            glm::vec3{0.0f, 0.0f, -1.0f});
        uniform->directions[4] = glm::lookAt(glm::vec3{0.0f, 0.0f, 0.0f},
            glm::vec3{0.0f, 0.0f, 1.0f},
            glm::vec3{0.0f, -1.0f, 0.0f});
        uniform->directions[5] = glm::lookAt(glm::vec3{0.0f, 0.0f, 0.0f},
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

    cubemap_sampler_ = create_sampler(backend_->device());

    cubemap_descriptor_layout_ =
        create_cubemap_descriptor_set_layout(backend_->device());

    vkrndr::create_descriptor_sets(backend_->device(),
        backend_->descriptor_pool(),
        std::span{&cubemap_descriptor_layout_, 1},
        std::span{&cubemap_descriptor_, 1});

    update_cubemap_descriptor(backend_->device(),
        cubemap_descriptor_,
        vkrndr::combined_sampler_descriptor(cubemap_sampler_, cubemap_texture_),
        vkrndr::buffer_descriptor(cubemap_uniform_buffer_));

    auto vertex_shader{vkrndr::create_shader_module(backend_->device(),
        "equirectangular_to_cubemap.vert.spv",
        VK_SHADER_STAGE_VERTEX_BIT,
        "main")};

    auto fragment_shader{vkrndr::create_shader_module(backend_->device(),
        "equirectangular_to_cubemap.frag.spv",
        VK_SHADER_STAGE_FRAGMENT_BIT,
        "main")};

    cubemap_pipeline_ =
        vkrndr::pipeline_builder_t{backend_->device(),
            vkrndr::pipeline_layout_builder_t{backend_->device()}
                .add_descriptor_set_layout(descriptor_layout_)
                .add_descriptor_set_layout(cubemap_descriptor_layout_)
                .add_push_constants<cubemap_push_constants_t>(
                    VK_SHADER_STAGE_VERTEX_BIT)
                .build(),
            cubemap_.format}
            .add_shader(as_pipeline_shader(vertex_shader))
            .add_shader(as_pipeline_shader(fragment_shader))
            .with_primitive_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .with_rasterization_samples(VK_SAMPLE_COUNT_1_BIT)
            .add_vertex_input(binding_description(), attribute_descriptions())
            .build();

    destroy(&backend_->device(), &vertex_shader);
    destroy(&backend_->device(), &fragment_shader);
}

void gltfviewer::environment_t::draw(VkCommandBuffer command_buffer,
    vkrndr::image_t const& color_image)
{
    VkViewport const viewport{.x = 0.0f,
        .y = 0.0f,
        .width = cppext::as_fp(cubemap_.extent.width),
        .height = cppext::as_fp(cubemap_.extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f};
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D const scissor{{0, 0}, cubemap_.extent};
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

    bind_on(command_buffer,
        *cubemap_pipeline_.layout,
        VK_PIPELINE_BIND_POINT_GRAPHICS);
    vkrndr::bind_pipeline(command_buffer,
        cubemap_pipeline_,
        1,
        std::span{&cubemap_descriptor_, 1});

    for (uint32_t i{}; i != cubemap_.face_views.size(); ++i)
    {
        cubemap_push_constants_t pc{.direction = i};
        vkCmdPushConstants(command_buffer,
            *cubemap_pipeline_.layout,
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(cubemap_push_constants_t),
            &pc);

        vkrndr::render_pass_t color_render_pass;
        color_render_pass.with_color_attachment(VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            cubemap_.face_views[i]);

        [[maybe_unused]] auto guard{
            color_render_pass.begin(command_buffer, {{0, 0}, cubemap_.extent})};

        vkCmdDrawIndexed(command_buffer, 36, 1, 0, 0, 0);
    }
}
