#ifndef GLTFVIEWER_SKYBOX_INCLUDED
#define GLTFVIEWER_SKYBOX_INCLUDED

#include <vkrndr_buffer.hpp>
#include <vkrndr_cubemap.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_pipeline.hpp>

#include <volk.h>

#include <filesystem>
#include <span>

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
        void load_hdr(std::filesystem::path const& hdr_image,
            VkDescriptorSetLayout environment_layout,
            VkFormat depth_buffer_format);

        void draw(VkCommandBuffer command_buffer,
            vkrndr::image_t const& color_image,
            vkrndr::image_t const& depth_buffer);

        [[nodiscard]] VkPipelineLayout pipeline_layout() const;

        [[nodiscard]] VkDescriptorImageInfo irradiance_info() const;

        [[nodiscard]] VkDescriptorImageInfo prefiltered_info() const;

        [[nodiscard]] VkDescriptorImageInfo brdf_lookup_info() const;

    public:
        skybox_t& operator=(skybox_t const&) = delete;

        skybox_t& operator=(skybox_t&&) noexcept = delete;

    private:
        void generate_cubemap_faces(VkDescriptorSetLayout layout,
            VkDescriptorSet descriptor_set);

        void generate_irradiance_map(VkDescriptorSetLayout layout,
            VkDescriptorSet descriptor_set);

        void generate_prefilter_map(VkDescriptorSetLayout layout,
            VkDescriptorSet descriptor_set);

        void generate_brdf_lookup();

        void render_to_cubemap(vkrndr::pipeline_t const& pipeline,
            std::span<VkDescriptorSet const> const& descriptors,
            vkrndr::cubemap_t& cubemap);

    private:
        vkrndr::backend_t* backend_;

        vkrndr::buffer_t cubemap_vertex_buffer_;
        vkrndr::buffer_t cubemap_index_buffer_;
        vkrndr::buffer_t cubemap_uniform_buffer_;

        vkrndr::cubemap_t cubemap_;
        vkrndr::cubemap_t irradiance_cubemap_;
        vkrndr::cubemap_t prefilter_cubemap_;
        vkrndr::image_t brdf_lookup_;

        VkSampler brdf_sampler_{VK_NULL_HANDLE};
        VkSampler skybox_sampler_{VK_NULL_HANDLE};
        VkDescriptorSetLayout skybox_descriptor_layout_{VK_NULL_HANDLE};
        VkDescriptorSet skybox_descriptor_{VK_NULL_HANDLE};
        vkrndr::pipeline_t skybox_pipeline_;
    };
} // namespace gltfviewer

#endif
