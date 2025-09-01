#include <environment.hpp>

#include <skybox.hpp>

#include <cppext_container.hpp>
#include <cppext_cycled_buffer.hpp>
#include <cppext_numeric.hpp>

#include <ngngfx_camera.hpp>
#include <ngngfx_orthographic_projection.hpp>
#include <ngngfx_projection.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_buffer.hpp>
#include <vkrndr_debug_utils.hpp>
#include <vkrndr_descriptors.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_memory.hpp>
#include <vkrndr_utility.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <imgui.h>

#include <vma_impl.hpp>

#include <volk.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

// IWYU pragma: no_include <glm/detail/func_exponential.inl>
// IWYU pragma: no_include <glm/detail/qualifier.hpp>
// IWYU pragma: no_include <glm/geometric.hpp>
// IWYU pragma: no_include <expected>
// IWYU pragma: no_include <string_view>

namespace
{
    struct [[nodiscard]] environment_uniform_t final
    {
        glm::mat4 view;
        glm::mat4 projection;
        glm::vec3 camera_position;
        uint32_t prefiltered_mip_levels;
        uint32_t light_count;
        uint8_t padding[12];
    };

    static_assert(sizeof(environment_uniform_t) % 16 == 0);

    struct [[nodiscard]] light_t final
    {
        glm::vec3 position;
        uint32_t type;
        glm::vec4 color;
        glm::mat4 light_space;
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

        VkDescriptorSetLayoutBinding irradiance_binding{};
        irradiance_binding.binding = 1;
        irradiance_binding.descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        irradiance_binding.descriptorCount = 1;
        irradiance_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding prefiltered_binding{};
        prefiltered_binding.binding = 2;
        prefiltered_binding.descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        prefiltered_binding.descriptorCount = 1;
        prefiltered_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding brdf_binding{};
        brdf_binding.binding = 3;
        brdf_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        brdf_binding.descriptorCount = 1;
        brdf_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array bindings{uniform_binding,
            irradiance_binding,
            prefiltered_binding,
            brdf_binding};

        return vkrndr::create_descriptor_set_layout(device, bindings).value();
    }

    void update_descriptor_set(vkrndr::device_t const& device,
        VkDescriptorSet const descriptor_set,
        VkDescriptorBufferInfo const uniform_info,
        VkDescriptorImageInfo const irradiance_info,
        VkDescriptorImageInfo const prefiltered_info,
        VkDescriptorImageInfo const brdf_info)
    {
        VkWriteDescriptorSet uniform_write{};
        uniform_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        uniform_write.dstSet = descriptor_set;
        uniform_write.dstBinding = 0;
        uniform_write.dstArrayElement = 0;
        uniform_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        uniform_write.descriptorCount = 1;
        uniform_write.pBufferInfo = &uniform_info;

        VkWriteDescriptorSet irradiance_write{};
        irradiance_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        irradiance_write.dstSet = descriptor_set;
        irradiance_write.dstBinding = 1;
        irradiance_write.dstArrayElement = 0;
        irradiance_write.descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        irradiance_write.descriptorCount = 1;
        irradiance_write.pImageInfo = &irradiance_info;

        VkWriteDescriptorSet prefiltered_write{};
        prefiltered_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        prefiltered_write.dstSet = descriptor_set;
        prefiltered_write.dstBinding = 2;
        prefiltered_write.dstArrayElement = 0;
        prefiltered_write.descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        prefiltered_write.descriptorCount = 1;
        prefiltered_write.pImageInfo = &prefiltered_info;

        VkWriteDescriptorSet brdf_write{};
        brdf_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        brdf_write.dstSet = descriptor_set;
        brdf_write.dstBinding = 3;
        brdf_write.dstArrayElement = 0;
        brdf_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        brdf_write.descriptorCount = 1;
        brdf_write.pImageInfo = &brdf_info;

        std::array const descriptor_writes{uniform_write,
            irradiance_write,
            prefiltered_write,
            brdf_write};

        vkUpdateDescriptorSets(device,
            vkrndr::count_cast(descriptor_writes.size()),
            descriptor_writes.data(),
            0,
            nullptr);
    }
} // namespace

gltfviewer::environment_t::environment_t(vkrndr::backend_t& backend)
    : backend_{&backend}
    , skybox_{backend}
    , light_projection_{{-20.0f, 20.0f}, {-10.0f, 10.0f}, {-10.0f, 10.0f}}
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

    for (auto& data : cppext::as_span(frame_data_))
    {
        auto const uniform_buffer_size{
            sizeof(environment_uniform_t) + sizeof(::light_t) * lights_.size()};

        data.uniform = create_buffer(backend_->device(),
            {.size = uniform_buffer_size,
                .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                .allocation_flags =
                    VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
                .required_memory_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT});
        VKRNDR_IF_DEBUG_UTILS(object_name(backend_->device(),
            data.uniform,
            "Environment Uniform"));

        data.uniform_map = vkrndr::map_memory(backend_->device(), data.uniform);

        vkrndr::check_result(allocate_descriptor_sets(backend_->device(),
            backend_->descriptor_pool(),
            cppext::as_span(descriptor_layout_),
            cppext::as_span(data.descriptor_set)));
    }
}

gltfviewer::environment_t::~environment_t()
{
    for (auto& data : cppext::as_span(frame_data_))
    {
        free_descriptor_sets(backend_->device(),
            backend_->descriptor_pool(),
            cppext::as_span(data.descriptor_set));

        unmap_memory(backend_->device(), &data.uniform_map);
        destroy(backend_->device(), data.uniform);
    }

    vkDestroyDescriptorSetLayout(backend_->device(),
        descriptor_layout_,
        nullptr);
}

VkDescriptorSetLayout gltfviewer::environment_t::descriptor_layout() const
{
    return descriptor_layout_;
}

bool gltfviewer::environment_t::has_directional_lights() const
{
    return std::ranges::any_of(lights_, &light_t::directional);
}

void gltfviewer::environment_t::load_skybox(
    std::filesystem::path const& hdr_image,
    VkFormat const depth_buffer_format)
{
    skybox_.load_hdr(hdr_image, descriptor_layout_, depth_buffer_format);

    for (auto const& data : cppext::as_span(frame_data_))
    {
        update_descriptor_set(backend_->device(),
            data.descriptor_set,
            vkrndr::buffer_descriptor(data.uniform),
            skybox_.irradiance_info(),
            skybox_.prefiltered_info(),
            skybox_.brdf_lookup_info());
    }
}

void gltfviewer::environment_t::draw_skybox(VkCommandBuffer command_buffer)
{
    bind_on(command_buffer,
        skybox_.pipeline_layout(),
        VK_PIPELINE_BIND_POINT_GRAPHICS);
    skybox_.draw(command_buffer);
}

void gltfviewer::environment_t::update()
{
    glm::vec2 near_far{light_projection_.near_far_planes()};
    float view_volume{light_projection_.left_right().y};

    ImGui::Begin("Lights");
    for (size_t i{}; i != lights_.size(); ++i)
    {
        ImGui::PushID(cppext::narrow<int>(i));
        ImGui::Checkbox("Enabled", &lights_[i].enabled);
        ImGui::Checkbox("Directional", &lights_[i].directional);
        ImGui::SliderFloat3("Position",
            glm::value_ptr(lights_[i].position),
            -100.0f,
            100.0f);
        ImGui::SliderFloat3("Color",
            glm::value_ptr(lights_[i].color),
            0.0f,
            10.0f);
        ImGui::PopID();
    }
    if (ImGui::SliderFloat2("Near / Far",
            glm::value_ptr(near_far),
            -100.0f,
            100.0f))
    {
        light_projection_.set_near_far_planes(near_far);
    }
    if (ImGui::SliderFloat("View volume", &view_volume, 1.0f, 35.0f, "%.1f"))
    {
        light_projection_.set_left_right(glm::vec2{-1.0f, 1.0f} * view_volume);
        light_projection_.set_bottom_top(glm::vec2{-1.0f, 1.0f} * view_volume);
    }
    ImGui::End();
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

void gltfviewer::environment_t::draw(ngngfx::camera_t const& camera,
    ngngfx::projection_t const& projection)
{
    frame_data_.cycle();

    auto* const header{frame_data_->uniform_map.as<environment_uniform_t>()};
    header->view = camera.view_matrix();
    header->projection = projection.projection_matrix();
    header->camera_position = camera.position();
    header->prefiltered_mip_levels = skybox_.prefiltered_mip_levels();

    auto* const light_array{
        frame_data_->uniform_map.as<::light_t>(sizeof(environment_uniform_t))};

    uint32_t enabled_lights{};
    for (light_t const& light : lights_)
    {
        if (light.enabled)
        {
            light_array[enabled_lights] = {
                .position = light.position,
                .type = static_cast<uint32_t>(light.directional),
                .color = glm::vec4{light.color, 0.0f},
            };

            if (light.directional)
            {
                light_projection_.update(glm::lookAtRH(light.position,
                    {0.0f, 0.0f, 0.0f},
                    {0.0f, 1.0f, 0.0f}));

                light_array[enabled_lights].light_space =
                    light_projection_.view_projection_matrix();
            }

            ++enabled_lights;
        }
    }
    header->light_count = enabled_lights;
}
