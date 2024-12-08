#include <vkgltf_model.hpp>

#include <vkrndr_buffer.hpp>
#include <vkrndr_image.hpp>

#include <glm/common.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/mat4x4.hpp>

namespace
{
    void calculate_node_matrices(vkgltf::model_t& model,
        vkgltf::node_t& node,
        glm::mat4 const& matrix)
    {
        node.matrix = matrix * node.matrix;
        for (vkgltf::node_t& child : node.children(model))
        {
            calculate_node_matrices(model, child, node.matrix);
        }
    }
} // namespace

vkgltf::bounding_box_t vkgltf::calculate_aabb(bounding_box_t const& box,
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

void vkgltf::destroy(vkrndr::device_t* const device, model_t* const model)
{
    if (model)
    {
        for (auto& image : model->images)
        {
            destroy(device, &image);
        }

        destroy(device, &model->index_buffer);
        destroy(device, &model->vertex_buffer);
    }
}

void vkgltf::make_node_matrices_absolute(model_t& model)
{
    for (scene_graph_t& scene : model.scenes)
    {
        for (node_t& node : scene.roots(model))
        {
            calculate_node_matrices(model, node, glm::mat4{1.0f});
        }
    }
}
