#ifndef VKGLTF_MODEL_INCLUDED
#define VKGLTF_MODEL_INCLUDED

#include <vkrndr_buffer.hpp>
#include <vkrndr_image.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <vulkan/vulkan_core.h>

#include <cstddef>
#include <cstdint>
#include <ranges>
#include <string>
#include <vector>

namespace vkrndr
{
    struct device_t;
} // namespace vkrndr

namespace vkgltf
{
    struct model_t;

    struct [[nodiscard]] vertex_t final
    {
        alignas(16) glm::vec3 position;
        glm::vec2 uv;
    };

    struct [[nodiscard]] sampler_info_t final
    {
        VkFilter mag_filter{VK_FILTER_NEAREST};
        VkFilter min_filter{VK_FILTER_NEAREST};
        VkSamplerAddressMode warp_u{VK_SAMPLER_ADDRESS_MODE_REPEAT};
        VkSamplerAddressMode warp_v{VK_SAMPLER_ADDRESS_MODE_REPEAT};
        VkSamplerMipmapMode mipmap_mode{VK_SAMPLER_MIPMAP_MODE_NEAREST};
    };

    struct [[nodiscard]] texture_t final
    {
        std::string name;
        size_t image_index;
        size_t sampler_index;
    };

    struct [[nodiscard]] pbr_metallic_roughness_t final
    {
        glm::vec4 base_color_factor{1.0f};
        texture_t* base_color_texture;
    };

    struct [[nodiscard]] material_t final
    {
        std::string name;
        pbr_metallic_roughness_t pbr_metallic_roughness;
    };

    struct [[nodiscard]] primitive_t final
    {
        VkPrimitiveTopology topology{VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

        uint32_t count;
        uint32_t first;

        bool is_indexed;
        int32_t vertex_offset{};

        size_t material_index;
    };

    struct [[nodiscard]] mesh_t final
    {
        std::string name;
        std::vector<primitive_t> primitives;
    };

    struct [[nodiscard]] node_t final
    {
        std::string name;

        mesh_t* mesh{nullptr};

        glm::mat4 matrix{1.0f};

        std::vector<size_t> child_indices;

        [[nodiscard]] constexpr auto children(model_t& model);

        [[nodiscard]] constexpr auto children(model_t const& model) const;
    };

    struct [[nodiscard]] scene_graph_t final
    {
        std::string name;

        std::vector<size_t> root_indices;

        [[nodiscard]] constexpr auto roots(model_t& model);

        [[nodiscard]] constexpr auto roots(model_t const& model) const;
    };

    struct [[nodiscard]] model_t final
    {
        vkrndr::buffer_t vertex_buffer;
        uint32_t vertex_count{};

        vkrndr::buffer_t index_buffer;
        uint32_t index_count{};

        std::vector<vkrndr::image_t> images;
        std::vector<sampler_info_t> samplers;

        std::vector<texture_t> textures;
        std::vector<material_t> materials;

        std::vector<mesh_t> meshes;
        std::vector<node_t> nodes;
        std::vector<scene_graph_t> scenes;
    };

    void destroy(vkrndr::device_t* device, model_t* model);
} // namespace vkgltf

constexpr auto vkgltf::node_t::children(vkgltf::model_t& model)
{
    return std::views::transform(child_indices,
        [&model](size_t const child) mutable { return model.nodes[child]; });
}

constexpr auto vkgltf::node_t::children(vkgltf::model_t const& model) const
{
    return std::views::transform(child_indices,
        [&model](size_t const child) { return model.nodes[child]; });
}

constexpr auto vkgltf::scene_graph_t::roots(vkgltf::model_t& model)
{
    return std::views::transform(root_indices,
        [&model](size_t const root) mutable { return model.nodes[root]; });
}

constexpr auto vkgltf::scene_graph_t::roots(vkgltf::model_t const& model) const
{
    return std::views::transform(root_indices,
        [&model](size_t const root) { return model.nodes[root]; });
}

#endif