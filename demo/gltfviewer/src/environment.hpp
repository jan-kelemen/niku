#ifndef GLTFVIEWER_ENVIRONMENT_INCLUDED
#define GLTFVIEWER_ENVIRONMENT_INCLUDED

#include <skybox.hpp>

#include <cppext_cycled_buffer.hpp>

#include <ngngfx_orthographic_projection.hpp>

#include <vkrndr_buffer.hpp>
#include <vkrndr_memory.hpp>

#include <glm/vec3.hpp>

#include <volk.h>

#include <filesystem>
#include <vector>

namespace ngngfx
{
    class camera_t;
    class projection_t;
} // namespace ngngfx

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

        [[nodiscard]] bool has_directional_lights() const;

        void load_skybox(std::filesystem::path const& hdr_image,
            VkFormat depth_buffer_format);

        void draw_skybox(VkCommandBuffer command_buffer);

        void update();

        void bind_on(VkCommandBuffer command_buffer,
            VkPipelineLayout layout,
            VkPipelineBindPoint bind_point);

        void draw(ngngfx::camera_t const& camera,
            ngngfx::projection_t const& projection);

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

        struct [[nodiscard]] light_t final
        {
            glm::vec3 position{0.0f, 2.0f, 0.0f};
            glm::vec3 color{5.0f, 5.0f, 5.0f};
            bool enabled{false};
            bool directional{false};
        };

    private:
        vkrndr::backend_t* backend_;

        skybox_t skybox_;

        ngngfx::orthographic_projection_t light_projection_;
        std::vector<light_t> lights_;

        VkDescriptorSetLayout descriptor_layout_{VK_NULL_HANDLE};

        cppext::cycled_buffer_t<frame_data_t> frame_data_;
    };
} // namespace gltfviewer

#endif
