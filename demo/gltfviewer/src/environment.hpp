#ifndef GLTFVIEWER_ENVIRONMENT_INCLUDED
#define GLTFVIEWER_ENVIRONMENT_INCLUDED

#include "vkrndr_pipeline.hpp"
#include <cppext_cycled_buffer.hpp>

#include <vkrndr_buffer.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_memory.hpp>

#include <glm/vec3.hpp>

#include <volk.h>

#include <filesystem>
#include <vector>

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

        void load_hdr(std::filesystem::path const& hdr_image);

        void draw(VkCommandBuffer command_buffer,
            vkrndr::image_t const& color_image);

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
            glm::vec3 position{0.0f, 1.0f, 0.0f};
            glm::vec3 color{1.0f, 1.0f, 1.0f};
            bool enabled{false};
        };

    private:
        vkrndr::backend_t* backend_;

        std::vector<light_t> lights_;

        vkrndr::image_t cubemap_texture_;
        vkrndr::buffer_t cubemap_vertex_buffer_;
        vkrndr::buffer_t cubemap_index_buffer_;
        VkSampler cubemap_sampler_;
        VkDescriptorSetLayout cubemap_descriptor_layout_;
        VkDescriptorSet cubemap_descriptor_;
        vkrndr::pipeline_t cubemap_pipeline_;

        VkDescriptorSetLayout descriptor_layout_{VK_NULL_HANDLE};

        cppext::cycled_buffer_t<frame_data_t> frame_data_;
    };
} // namespace gltfviewer

#endif
