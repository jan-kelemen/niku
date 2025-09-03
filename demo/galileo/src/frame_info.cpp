#include <frame_info.hpp>

#include <cppext_container.hpp>
#include <cppext_cycled_buffer.hpp>

#include <ngnast_scene_model.hpp>

#include <ngngfx_camera.hpp>
#include <ngngfx_projection.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_buffer.hpp>
#include <vkrndr_descriptors.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_memory.hpp>
#include <vkrndr_utility.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <vma_impl.hpp>

#include <volk.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <random>
#include <span>

// IWYU pragma: no_include <expected>

namespace
{
    constexpr uint32_t max_lights{2000};

    struct [[nodiscard]] gpu_frame_info_t final
    {
        glm::mat4 view;
        glm::mat4 projection;
        glm::vec3 position;
        uint32_t light_count;
    };

    struct [[nodiscard]] gpu_light_t final
    {
        glm::vec4 color;
        glm::vec3 position;
        uint32_t padding;
    };

    [[nodiscard]] VkDescriptorSetLayout create_descriptor_set_layout(
        vkrndr::device_t const& device)
    {
        VkDescriptorSetLayoutBinding info_binding{};
        info_binding.binding = 0;
        info_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        info_binding.descriptorCount = 1;
        info_binding.stageFlags =
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding light_binding{};
        light_binding.binding = 1;
        light_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        light_binding.descriptorCount = 1;
        light_binding.stageFlags =
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array bindings{info_binding, light_binding};

        return vkrndr::create_descriptor_set_layout(device, bindings).value();
    }

    void update_descriptor_set(vkrndr::device_t const& device,
        VkDescriptorSet const descriptor_set,
        VkDescriptorBufferInfo const info_buffer,
        VkDescriptorBufferInfo const light_buffer)
    {
        VkWriteDescriptorSet info_write{};
        info_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        info_write.dstSet = descriptor_set;
        info_write.dstBinding = 0;
        info_write.dstArrayElement = 0;
        info_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        info_write.descriptorCount = 1;
        info_write.pBufferInfo = &info_buffer;

        VkWriteDescriptorSet light_write{};
        light_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        light_write.dstSet = descriptor_set;
        light_write.dstBinding = 1;
        light_write.dstArrayElement = 0;
        light_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        light_write.descriptorCount = 1;
        light_write.pBufferInfo = &light_buffer;

        std::array const descriptor_writes{info_write, light_write};

        vkUpdateDescriptorSets(device,
            vkrndr::count_cast(descriptor_writes),
            descriptor_writes.data(),
            0,
            nullptr);
    }

    void generate_lights(ngnast::bounding_box_t const& bb,
        std::span<gpu_light_t> buffer)
    {
        std::default_random_engine engine;
        std::uniform_real_distribution pdist{bb.min.x, bb.max.x};
        std::uniform_real_distribution cdist{0.0f, 1.0f};
        std::uniform_real_distribution hdist{bb.max.y, 5.0f};

        std::ranges::generate(buffer,
            [&]() -> gpu_light_t
            {
                return {.color = {cdist(engine),
                            cdist(engine),
                            cdist(engine),
                            1.0f},
                    .position = {pdist(engine), hdist(engine), pdist(engine)},
                    .padding = 0};
            });
    }
} // namespace

galileo::frame_info_t::frame_info_t(vkrndr::backend_t& backend)
    : backend_{&backend}
    , descriptor_set_layout_{create_descriptor_set_layout(backend_->device())}
    , frame_data_{backend_->frames_in_flight(), backend_->frames_in_flight()}
{
    for (auto& data : cppext::as_span(frame_data_))
    {
        data.info_buffer = vkrndr::create_buffer(backend_->device(),
            {.size = sizeof(gpu_frame_info_t),
                .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                .allocation_flags =
                    VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
                .required_memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT});
        data.info_map =
            vkrndr::map_memory(backend_->device(), data.info_buffer);

        data.light_buffer = vkrndr::create_buffer(backend_->device(),
            {.size = sizeof(gpu_light_t) * max_lights,
                .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .required_memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT});

        vkrndr::check_result(allocate_descriptor_sets(backend_->device(),
            backend_->descriptor_pool(),
            cppext::as_span(descriptor_set_layout_),
            cppext::as_span(data.descriptor_set)));

        update_descriptor_set(backend_->device(),
            data.descriptor_set,
            vkrndr::buffer_descriptor(data.info_buffer),
            vkrndr::buffer_descriptor(data.light_buffer));
    }
}

galileo::frame_info_t::~frame_info_t()
{
    for (auto& data : cppext::as_span(frame_data_))
    {
        free_descriptor_sets(backend_->device(),
            backend_->descriptor_pool(),
            cppext::as_span(data.descriptor_set));

        vkrndr::destroy(backend_->device(), data.light_buffer);

        vkrndr::unmap_memory(backend_->device(), &data.info_map);

        vkrndr::destroy(backend_->device(), data.info_buffer);
    }

    vkDestroyDescriptorSetLayout(backend_->device(),
        descriptor_set_layout_,
        nullptr);
}

VkDescriptorSetLayout galileo::frame_info_t::descriptor_set_layout() const
{
    return descriptor_set_layout_;
}

void galileo::frame_info_t::begin_frame() { frame_data_.cycle(); }

void galileo::frame_info_t::update(ngngfx::camera_t const& camera,
    ngngfx::projection_t const& projection,
    uint32_t const light_count)
{
    auto* const gpu{frame_data_->info_map.as<gpu_frame_info_t>()};
    gpu->view = camera.view_matrix();
    gpu->projection = projection.projection_matrix();
    gpu->position = camera.position();
    gpu->light_count = light_count;
}

void galileo::frame_info_t::bind_on(VkCommandBuffer command_buffer,
    VkPipelineLayout layout,
    VkPipelineBindPoint const bind_point)
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

void galileo::frame_info_t::disperse_lights(
    ngnast::bounding_box_t const& bounding_box)
{
    vkrndr::buffer_t const staging{
        vkrndr::create_staging_buffer(backend_->device(),
            sizeof(gpu_light_t) * max_lights)};
    {
        vkrndr::mapped_memory_t staging_map{
            map_memory(backend_->device(), staging)};
        generate_lights(bounding_box,
            std::span{staging_map.as<gpu_light_t>(), max_lights});
        unmap_memory(backend_->device(), &staging_map);
    }

    for (auto& data : cppext::as_span(frame_data_))
    {
        backend_->transfer_buffer(staging, data.light_buffer);
    }

    destroy(backend_->device(), staging);
}
