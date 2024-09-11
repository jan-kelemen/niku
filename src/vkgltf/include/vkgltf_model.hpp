#ifndef VKGLTF_MODEL_INCLUDED
#define VKGLTF_MODEL_INCLUDED

#include <vkrndr_buffer.hpp>

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
        glm::vec3 position;
    };

    struct [[nodiscard]] primitive_t final
    {
        VkPrimitiveTopology topology{VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

        uint32_t count;
        uint32_t first;

        bool is_indexed;
        int32_t vertex_offset{};
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

        std::vector<mesh_t> meshes;
        std::vector<node_t> nodes;
        std::vector<scene_graph_t> scenes;
    };

    void destroy(vkrndr::device_t* device, model_t* model);

    constexpr auto vkgltf::node_t::children(model_t& model)
    {
        return std::views::transform(child_indices,
            [&model](size_t const child) mutable
            { return model.nodes[child]; });
    }

    constexpr auto vkgltf::node_t::children(model_t const& model) const
    {
        return std::views::transform(child_indices,
            [&model](size_t const child) { return model.nodes[child]; });
    }

    constexpr auto vkgltf::scene_graph_t::roots(model_t& model)
    {
        return std::views::transform(root_indices,
            [&model](size_t const root) mutable { return model.nodes[root]; });
    }

    constexpr auto vkgltf::scene_graph_t::roots(model_t const& model) const
    {
        return std::views::transform(root_indices,
            [&model](size_t const root) { return model.nodes[root]; });
    }

} // namespace vkgltf

#endif
