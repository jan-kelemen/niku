#ifndef GLTFVIEWER_SKYBOX_INCLUDED
#define GLTFVIEWER_SKYBOX_INCLUDED

#include <vkrndr_buffer.hpp>
#include <vkrndr_cubemap.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_memory.hpp>
#include <vkrndr_pipeline.hpp>

#include <glm/vec3.hpp>

#include <volk.h>

#include <filesystem>
#include <vector>

namespace vkrndr
{
    class backend_t;
} // namespace vkrndr

namespace gltfviewer
{
    class [[nodiscard]] skybox_t final
    {
    public:
        explicit skybox_t(vkrndr::backend_t& backend);

        skybox_t(skybox_t const&) = delete;

        skybox_t(skybox_t&&) noexcept = delete;

    public:
        ~skybox_t();

    public:
        void load_hdr(std::filesystem::path const& hdr_image);

        void draw(VkCommandBuffer command_buffer,
            vkrndr::image_t const& color_image);

    public:
        skybox_t& operator=(skybox_t const&) = delete;

        skybox_t& operator=(skybox_t&&) noexcept = delete;

    private:
        vkrndr::backend_t* backend_;

        vkrndr::image_t cubemap_texture_;
        vkrndr::cubemap_t cubemap_;
        vkrndr::buffer_t cubemap_vertex_buffer_;
        vkrndr::buffer_t cubemap_index_buffer_;
        vkrndr::buffer_t cubemap_uniform_buffer_;
        VkSampler cubemap_sampler_;
        VkDescriptorSetLayout cubemap_descriptor_layout_;
        VkDescriptorSet cubemap_descriptor_;
        vkrndr::pipeline_t cubemap_pipeline_;
    };
} // namespace gltfviewer

#endif
