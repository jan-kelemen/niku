#ifndef GLTFVIEWER_ENVIRONMENT_INCLUDED
#define GLTFVIEWER_ENVIRONMENT_INCLUDED

#include <cppext_cycled_buffer.hpp>

#include <vkrndr_buffer.hpp>
#include <vkrndr_memory.hpp>

#include <glm/vec3.hpp>

#include <volk.h>

namespace niku
{
    class camera_t;
} // namespace niku

namespace vkrndr
{
    class backend_t;
} // namespace vkrndr

namespace gltfviewer
{
    class [[nodiscard]] environment_t final
    {
    public:
        explicit environment_t(vkrndr::backend_t& backend);

        environment_t(environment_t const&) = delete;

        environment_t(environment_t&&) noexcept = delete;

    public:
        ~environment_t();

    public:
        [[nodiscard]] VkDescriptorSetLayout descriptor_layout() const;

        void update(niku::camera_t const& camera);

        void bind_on(VkCommandBuffer command_buffer,
            VkPipelineLayout layout,
            VkPipelineBindPoint bind_point);

    public:
        environment_t& operator=(environment_t const&) = delete;

        environment_t& operator=(environment_t&&) noexcept = delete;

    private:
        struct [[nodiscard]] frame_data_t final
        {
            vkrndr::buffer_t uniform;
            vkrndr::mapped_memory_t uniform_map;
            VkDescriptorSet descriptor_set{VK_NULL_HANDLE};
        };

    private:
        vkrndr::backend_t* backend_;

        VkDescriptorSetLayout descriptor_layout_{VK_NULL_HANDLE};

        cppext::cycled_buffer_t<frame_data_t> frame_data_;

        glm::vec3 light_position_{0.0f, 1.0f, 0.0f};
        glm::vec3 light_color_{1.0f};
    };
} // namespace gltfviewer

#endif