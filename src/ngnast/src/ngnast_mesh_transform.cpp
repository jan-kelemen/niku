#include <ngnast_mesh_transform.hpp>

#include <ngnast_scene_model.hpp>

#include <cppext_pragma_warning.hpp>

#include <meshoptimizer.h>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <utility>
#include <vector>

bool ngnast::mesh::make_unindexed(primitive_t& primitive)
{
    if (primitive.indices.empty())
    {
        return false;
    }

    std::vector<vertex_t> new_vertices;
    new_vertices.reserve(primitive.indices.size());
    std::ranges::transform(primitive.indices,
        std::back_inserter(new_vertices),
        [&primitive](unsigned int const i) { return primitive.vertices[i]; });

    primitive.vertices = std::move(new_vertices);
    primitive.indices.clear();

    return true;
}

bool ngnast::mesh::make_indexed(primitive_t& primitive)
{
    if (!primitive.indices.empty())
    {
        return false;
    }

    size_t const index_count{primitive.vertices.size()};
    size_t const unindexed_vertex_count{primitive.vertices.size()};

    std::vector<unsigned int> remap;

    DISABLE_WARNING_PUSH
    DISABLE_WARNING_NULL_DEREFERENCE
    remap.resize(index_count);
    DISABLE_WARNING_POP

    size_t const vertex_count{meshopt_generateVertexRemap(remap.data(),
        nullptr,
        index_count,
        primitive.vertices.data(),
        unindexed_vertex_count,
        sizeof(vertex_t))};

    std::vector<vertex_t> new_vertices;
    new_vertices.resize(vertex_count);
    meshopt_remapVertexBuffer(new_vertices.data(),
        primitive.vertices.data(),
        unindexed_vertex_count,
        sizeof(vertex_t),
        remap.data());
    primitive.vertices = std::move(new_vertices);

    primitive.indices.resize(index_count);
    meshopt_remapIndexBuffer(primitive.indices.data(),
        nullptr,
        index_count,
        remap.data());

    return true;
}
