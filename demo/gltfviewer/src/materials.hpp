#ifndef GLTFVIEWER_MATERIALS_INCLUDED
#define GLTFVIEWER_MATERIALS_INCLUDED

#include <vkrndr_buffer.hpp>
#include <vkrndr_image.hpp>

#include <volk.h>

#include <vector>

namespace vkgltf
{
    struct model_t;
} // namespace vkgltf

namespace vkrndr
{
    class backend_t;
} // namespace vkrndr

namespace gltfviewer
{
    class [[nodiscard]] materials_t final
    {
    public:
        explicit materials_t(vkrndr::backend_t& backend);

        materials_t(materials_t const&) = delete;

        materials_t(materials_t&&) noexcept = delete;

    public:
        ~materials_t();

    public:
        [[nodiscard]] VkDescriptorSetLayout descriptor_layout() const;

        void load(vkgltf::model_t& model);

        void bind_on(VkCommandBuffer command_buffer,
            VkPipelineLayout layout,
            VkPipelineBindPoint bind_point);

    public:
        materials_t& operator=(materials_t const&) = delete;

        materials_t& operator=(materials_t&&) noexcept = delete;

    private:
        void create_dummy_material();

        void clear();

    private:
        vkrndr::backend_t* backend_;

        bool has_materials_{false};

        // Dummy data when materials don't exist
        VkDescriptorSetLayout dummy_descriptor_layout_{VK_NULL_HANDLE};
        VkDescriptorSet dummy_descriptor_set_{VK_NULL_HANDLE};
        vkrndr::buffer_t dummy_uniform_;
        VkSampler default_sampler_{VK_NULL_HANDLE};
        vkrndr::image_t white_pixel_;

        // Actual model data
        VkDescriptorSetLayout descriptor_layout_{VK_NULL_HANDLE};
        VkDescriptorSet descriptor_set_{VK_NULL_HANDLE};
        vkrndr::buffer_t uniform_;
        std::vector<VkSampler> samplers_;
        std::vector<vkrndr::image_t> images_;
    };
} // namespace gltfviewer

#endif
