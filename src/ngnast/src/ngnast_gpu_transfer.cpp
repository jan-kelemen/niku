#include <ngnast_gpu_transfer.hpp>

#include <vkrndr_device.hpp>
#include <vkrndr_memory.hpp>

#include <cppext_numeric.hpp>

#include <boost/dynamic_bitset/dynamic_bitset.hpp>
#include <boost/scope/defer.hpp>
#include <boost/scope/scope_exit.hpp>

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <numeric>
#include <vector>

namespace
{
    ngnast::gpu::vertex_t to_gpu_vertex(ngnast::vertex_t const& v)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
        return {.position = glm::make_vec3(v.position),
            .normal = v.normal,
            .tangent = v.tangent,
            .color = v.color,
            .uv = v.uv};
    }

    void load_node(ngnast::scene_model_t const& model,
        size_t const node_index,
        boost::dynamic_bitset<>& mesh_consumed,
        std::vector<ngnast::gpu::primitive_t>& primitives,
        ngnast::gpu::vertex_t*& vertices,
        int32_t& running_vertex_count,
        uint32_t*& indices,
        uint32_t& running_index_count)
    {
        auto const& node{model.nodes[node_index]};

        if (auto const mi{node.mesh_index}; mi && !mesh_consumed[*mi])
        {
            auto& mesh{model.meshes[*mi]};

            std::ranges::transform(mesh.primitive_indices,
                std::back_inserter(primitives),
                [&](size_t const pi) mutable
                {
                    ngnast::primitive_t const& p{model.primitives[pi]};

                    ngnast::gpu::primitive_t rv{.topology = p.topology,
                        .material_index = p.material_index,
                        .vertex_count =
                            cppext::narrow<uint32_t>(p.vertices.size()),
                        .is_indexed = !p.indices.empty()};

                    // clang-format off
                    vertices = std::ranges::transform(p.vertices,
                        vertices,
                        to_gpu_vertex).out;
                    // clang-format on

                    if constexpr (std::is_same_v<uint32_t, unsigned int>)
                    {
                        indices = std::ranges::copy(p.indices, indices).out;
                    }
                    else
                    {
                        // clang-format off
                        indices = std::ranges::transform(p.indices,
                            indices,
                            cppext::narrow<uint32_t, unsigned int>).out;
                        // clang-format on
                    }

                    if (rv.is_indexed)
                    {
                        rv.count = cppext::narrow<uint32_t>(p.indices.size());
                        rv.first =
                            cppext::narrow<uint32_t>(running_index_count);
                        rv.vertex_offset =
                            cppext::narrow<int32_t>(running_vertex_count);
                    }
                    else
                    {
                        rv.count = cppext::narrow<uint32_t>(p.vertices.size());
                        rv.vertex_offset =
                            cppext::narrow<int32_t>(running_vertex_count);
                        rv.first =
                            cppext::narrow<uint32_t>(running_vertex_count);
                    }

                    running_index_count +=
                        cppext::narrow<uint32_t>(p.indices.size());
                    running_vertex_count +=
                        cppext::narrow<int32_t>(p.vertices.size());

                    return rv;
                });

            mesh_consumed.set(*mi);
        }

        for (size_t const i : node.child_indices)
        {
            load_node(model,
                i,
                mesh_consumed,
                primitives,
                vertices,
                running_vertex_count,
                indices,
                running_index_count);
        }
    }

    void load_scenes(ngnast::scene_model_t const& model,
        std::vector<ngnast::gpu::primitive_t>& primitives,
        ngnast::gpu::vertex_t* vertices,
        uint32_t* indices)
    {
        boost::dynamic_bitset<> mesh_consumed{model.meshes.size()};
        int32_t running_vertex_count{};
        uint32_t running_index_count{};

        for (ngnast::scene_graph_t const& scene : model.scenes)
        {
            for (size_t const node_index : scene.root_indices)
            {
                load_node(model,
                    node_index,
                    mesh_consumed,
                    primitives,
                    vertices,
                    running_vertex_count,
                    indices,
                    running_index_count);
            }
        }
    }
} // namespace

void ngnast::gpu::destroy(vkrndr::device_t* const device,
    geometry_transfer_result_t* const model)
{
    if (model)
    {
        destroy(device, &model->vertex_buffer);
        destroy(device, &model->index_buffer);
    }
}

ngnast::gpu::geometry_transfer_result_t ngnast::gpu::transfer_geometry(
    vkrndr::device_t& device,
    scene_model_t const& model)
{
    geometry_transfer_result_t rv;

    std::ranges::for_each(model.meshes,
        [&model, &vc = rv.vertex_count, &ic = rv.index_count](
            mesh_t const& mesh)
        {
            std::ranges::for_each(mesh.primitive_indices,
                [&](size_t const index)
                {
                    auto const& primitive{model.primitives[index]};

                    vc += cppext::narrow<uint32_t>(primitive.vertices.size());
                    ic += cppext::narrow<uint32_t>(primitive.indices.size());
                });
        });

    rv.vertex_buffer = vkrndr::create_staging_buffer(device,
        sizeof(gpu::vertex_t) * rv.vertex_count);
    auto vertex_map{vkrndr::map_memory(device, rv.vertex_buffer)};
    boost::scope::defer_guard const unmap_vertex{
        [&device, &vertex_map]() { unmap_memory(device, &vertex_map); }};

    auto* const vertices{vertex_map.as<gpu::vertex_t>()};

    vkrndr::mapped_memory_t index_map;
    boost::scope::scope_exit unmap_index{
        [&device, &index_map]() { unmap_memory(device, &index_map); }};

    uint32_t* indices{};
    if (rv.index_count)
    {
        rv.index_buffer = vkrndr::create_staging_buffer(device,
            sizeof(uint32_t) * rv.index_count);
        index_map = vkrndr::map_memory(device, rv.index_buffer);
        indices = index_map.as<uint32_t>();
    }
    else
    {
        unmap_index.set_active(false);
    }

    load_scenes(model, rv.primitives, vertices, indices);

    return rv;
}
