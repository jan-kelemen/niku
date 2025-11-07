#include <ngnast_scene_model.hpp>

#include <cppext_numeric.hpp>

#include <glm/common.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/mat4x4.hpp>

#include <algorithm>

namespace
{
    void calculate_node_matrices(ngnast::scene_model_t& model,
        ngnast::node_t& node,
        glm::mat4 const& matrix)
    {
        node.matrix = matrix * node.matrix;
        for (ngnast::node_t& child : node.children(model))
        {
            calculate_node_matrices(model, child, node.matrix);
        }
    }

    [[nodiscard]] uint32_t nodes_with_mesh(ngnast::node_t const& node,
        ngnast::scene_model_t const& model,
        bool const include_primitives)
    {
        auto rv{static_cast<uint32_t>(node.mesh_index.has_value())};
        if (rv && include_primitives)
        {
            rv *= cppext::narrow<uint32_t>(
                model.meshes[*node.mesh_index].primitive_indices.size());
        }

        // cppcheck-suppress-begin useStlAlgorithm
        for (auto const& child : node.children(model))
        {
            rv += nodes_with_mesh(child, model, include_primitives);
        }
        // cppcheck-suppress-end useStlAlgorithm
        return rv;
    }
} // namespace

ngnast::bounding_box_t ngnast::calculate_aabb(bounding_box_t const& box,
    glm::mat4 const& matrix)
{
    glm::vec3 min{column(matrix, 3)};
    glm::vec3 max{min};

    glm::vec3 const right{column(matrix, 0)};
    glm::vec3 v0{right * box.min.x};
    glm::vec3 v1{right * box.max.x};
    min += glm::min(v0, v1);
    max += glm::max(v0, v1);

    glm::vec3 const up{column(matrix, 1)};
    v0 = up * box.min.y;
    v1 = up * box.max.y;
    min += glm::min(v0, v1);
    max += glm::max(v0, v1);

    glm::vec3 const back{column(matrix, 2)};
    v0 = back * box.min.z;
    v1 = back * box.max.z;
    min += glm::min(v0, v1);
    max += glm::max(v0, v1);

    return {min, max};
}

void ngnast::make_node_matrices_absolute(scene_model_t& model)
{
    for (scene_graph_t& scene : model.scenes)
    {
        for (node_t& node : scene.roots(model))
        {
            calculate_node_matrices(model, node, glm::mat4{1.0f});
        }
    }
}

void ngnast::assign_default_material_index(scene_model_t& model,
    size_t const index)
{
    std::ranges::for_each(
        model.primitives,
        [index](std::optional<size_t>& p)
        {
            if (!p)
            {
                p = index;
            }
        },
        &ngnast::primitive_t::material_index);
}

uint32_t ngnast::required_transforms(scene_model_t const& model,
    bool const include_primitives)
{
    uint32_t rv{};
    // cppcheck-suppress-begin useStlAlgorithm
    for (auto const& graph : model.scenes)
    {
        for (auto const& root : graph.roots(model))
        {
            rv += nodes_with_mesh(root, model, include_primitives);
        }
    }
    // cppcheck-suppress-end useStlAlgorithm
    return rv;
}
