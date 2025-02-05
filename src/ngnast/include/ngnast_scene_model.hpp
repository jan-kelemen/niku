#ifndef NGNAST_SCENE_MODEL_INCLUDED
#define NGNAST_SCENE_MODEL_INCLUDED

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <volk.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <vector>

// IWYU pragma: no_include <glm/detail/qualifier.hpp>

namespace ngnast
{
    struct scene_model_t;

    struct [[nodiscard]] bounding_box_t final
    {
        glm::vec3 min{};
        glm::vec3 max{};
    };

    bounding_box_t calculate_aabb(bounding_box_t const& box,
        glm::mat4 const& matrix);

    struct [[nodiscard]] vertex_t final
    {
        glm::vec3 position{};
        glm::vec3 normal{};
        glm::vec4 tangent{};
        glm::vec4 color{1.0f};
        glm::vec2 uv{};
    };

    struct [[nodiscard]] sampler_info_t final
    {
        VkFilter mag_filter{VK_FILTER_LINEAR};
        VkFilter min_filter{VK_FILTER_LINEAR};
        VkSamplerAddressMode wrap_u{VK_SAMPLER_ADDRESS_MODE_REPEAT};
        VkSamplerAddressMode wrap_v{VK_SAMPLER_ADDRESS_MODE_REPEAT};
        VkSamplerMipmapMode mipmap_mode{VK_SAMPLER_MIPMAP_MODE_LINEAR};
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
        texture_t* base_color_texture{};
        texture_t* metallic_roughness_texture{};
        float metallic_factor{1.0f};
        float roughness_factor{1.0f};
    };

    enum class alpha_mode_t
    {
        opaque = 1,
        mask = 2,
        blend = 4,
    };

    struct [[nodiscard]] material_t final
    {
        std::string name;
        pbr_metallic_roughness_t pbr_metallic_roughness;
        texture_t* normal_texture{};
        texture_t* emissive_texture{};
        texture_t* occlusion_texture{};
        float normal_scale{1.0f};
        glm::vec3 emissive_factor{};
        float emissive_strength{1.0f};
        float occlusion_strength{1.0f};
        alpha_mode_t alpha_mode{alpha_mode_t::opaque};
        float alpha_cutoff{0.0f};
        bool double_sided{false};
    };

    struct [[nodiscard]] primitive_t final
    {
        VkPrimitiveTopology topology{VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

        std::vector<vertex_t> vertices;
        std::vector<unsigned int> indices;

        std::optional<size_t> material_index;

        bounding_box_t bounding_box;
    };

    struct [[nodiscard]] mesh_t final
    {
        std::string name;
        std::vector<size_t> primitive_indices;
        bounding_box_t bounding_box;
    };

    struct [[nodiscard]] node_t final
    {
        std::string name;

        std::optional<size_t> mesh_index;

        glm::mat4 matrix{1.0f};

        bounding_box_t aabb;

        std::vector<size_t> child_indices;

        [[nodiscard]] constexpr auto children(scene_model_t& model);

        [[nodiscard]] constexpr auto children(scene_model_t const& model) const;
    };

    struct [[nodiscard]] scene_graph_t final
    {
        std::string name;

        std::vector<size_t> root_indices;

        [[nodiscard]] constexpr auto roots(scene_model_t& model);

        [[nodiscard]] constexpr auto roots(scene_model_t const& model) const;
    };

    struct [[nodiscard]] image_t final
    {
        std::unique_ptr<std::byte[], void (*)(std::byte*)> data;
        size_t data_size{};
        VkExtent2D extent;
        VkFormat format;
    };

    struct [[nodiscard]] scene_model_t final
    {
        std::vector<image_t> images;
        std::vector<sampler_info_t> samplers;

        std::vector<texture_t> textures;
        std::vector<material_t> materials;

        std::vector<primitive_t> primitives;
        std::vector<mesh_t> meshes;
        std::vector<node_t> nodes;
        std::vector<scene_graph_t> scenes;
    };

    void make_node_matrices_absolute(scene_model_t& model);

    void assign_default_material_index(scene_model_t& model, size_t index);
} // namespace ngnast

constexpr auto ngnast::node_t::children(ngnast::scene_model_t& model)
{
    return std::views::transform(child_indices,
        [&model](size_t const child) mutable -> node_t&
        { return model.nodes[child]; });
}

constexpr auto ngnast::node_t::children(
    ngnast::scene_model_t const& model) const
{
    return std::views::transform(child_indices,
        [&model](size_t const child) -> node_t const&
        { return model.nodes[child]; });
}

constexpr auto ngnast::scene_graph_t::roots(ngnast::scene_model_t& model)
{
    return std::views::transform(root_indices,
        [&model](size_t const root) mutable -> node_t&
        { return model.nodes[root]; });
}

constexpr auto ngnast::scene_graph_t::roots(
    ngnast::scene_model_t const& model) const
{
    return std::views::transform(root_indices,
        [&model](size_t const root) -> node_t const&
        { return model.nodes[root]; });
}

#endif
