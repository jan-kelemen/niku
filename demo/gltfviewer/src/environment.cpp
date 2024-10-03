#include <environment.hpp>

#include <cppext_cycled_buffer.hpp>

#include <niku_camera.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_buffer.hpp>
#include <vkrndr_descriptors.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_memory.hpp>
#include <vkrndr_utility.hpp>

#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <imgui.h>

#include <volk.h>

#include <array>
#include <span>

namespace
{
    struct [[nodiscard]] environment_uniform_t final
    {
        glm::mat4 view;
        glm::mat4 projection;
        glm::vec3 camera_position;
        uint8_t padding1[4];
        // TODO-JK: Remove lights from this
        glm::vec3 light_position;
        uint8_t padding2[4];
        glm::vec3 light_color;
        uint8_t padding3[4];
    };

    static_assert(sizeof(environment_uniform_t) % 16 == 0);

    [[nodiscard]] VkDescriptorSetLayout create_descriptor_set_layout(
        vkrndr::device_t const& device)
    {
        VkDescriptorSetLayoutBinding uniform_binding{};
        uniform_binding.binding = 0;
        uniform_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uniform_binding.descriptorCount = 1;
        uniform_binding.stageFlags =
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array const bindings{uniform_binding};

        VkDescriptorSetLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = vkrndr::count_cast(bindings.size());
        layout_info.pBindings = bindings.data();

        VkDescriptorSetLayout rv; // NOLINT
        vkrndr::check_result(vkCreateDescriptorSetLayout(device.logical,
            &layout_info,
            nullptr,
            &rv));

        return rv;
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
        uniform_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uniform_write.descriptorCount = 1;
        uniform_write.pBufferInfo = &uniform_info;

        std::array const descriptor_writes{uniform_write};

        vkUpdateDescriptorSets(device.logical,
            vkrndr::count_cast(descriptor_writes.size()),
            descriptor_writes.data(),
            0,
            nullptr);
    }
} // namespace

gltfviewer::environment_t::environment_t(vkrndr::backend_t& backend)
    : backend_{&backend}
    , descriptor_layout_{create_descriptor_set_layout(backend_->device())}
    , frame_data_{backend_->frames_in_flight(), backend_->frames_in_flight()}
{
    for (auto& data : frame_data_.as_span())
    {
        auto const uniform_buffer_size{sizeof(environment_uniform_t)};
        data.uniform = create_buffer(backend_->device(),
            uniform_buffer_size,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
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
}

VkDescriptorSetLayout gltfviewer::environment_t::descriptor_layout() const
{
    return descriptor_layout_;
}

void gltfviewer::environment_t::update(niku::camera_t const& camera)
{
    frame_data_.cycle();

    // TODO-JK: TEMP
    ImGui::Begin("Light");
    ImGui::SliderFloat3("Position",
        glm::value_ptr(light_position_),
        -500.0f,
        500.0f);
    ImGui::SliderFloat3("Color", glm::value_ptr(light_color_), 0.0f, 1.0f);
    ImGui::End();

    auto* const uniform{frame_data_->uniform_map.as<environment_uniform_t>()};
    uniform->view = camera.view_matrix();
    uniform->projection = camera.projection_matrix();
    uniform->camera_position = camera.position();
    uniform->light_position = light_position_;
    uniform->light_color = light_color_;
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
