#include <ngnast_gpu_transfer.hpp>

#include <ngnast_scene_model.hpp>

#include <vkrndr_acceleration_structure.hpp>
#include <vkrndr_backend.hpp>
#include <vkrndr_buffer.hpp>
#include <vkrndr_memory.hpp>
#include <vkrndr_synchronization.hpp>
#include <vkrndr_transient_operation.hpp>
#include <vkrndr_utility.hpp>

#include <cppext_numeric.hpp>

#include <boost/scope/defer.hpp>
#include <boost/scope/scope_exit.hpp>
#include <boost/scope/scope_fail.hpp>

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cassert>
#include <iterator>
#include <optional>
#include <type_traits>
#include <vector>
#include <vulkan/utility/vk_struct_helper.hpp>

namespace
{
    ngnast::gpu::vertex_t to_gpu_vertex(ngnast::vertex_t const& v)
    {
        return {.position = glm::make_vec3(v.position),
            .normal = v.normal,
            .tangent = v.tangent,
            .color = v.color,
            .uv = v.uv};
    }

    [[nodiscard]] std::vector<ngnast::gpu::primitive_t> transfer_primitives(
        ngnast::scene_model_t const& model,
        ngnast::gpu::vertex_t* vertices,
        uint32_t* indices)
    {
        int32_t running_vertex_count{};
        uint32_t running_index_count{};

        std::vector<ngnast::gpu::primitive_t> rv;
        rv.reserve(model.primitives.size());

        std::ranges::transform(model.primitives,
            std::back_inserter(rv),
            [&](ngnast::primitive_t const& p) mutable
            {
                ngnast::gpu::primitive_t gp{.topology = p.topology,
                    .material_index = p.material_index.value_or(0),
                    .vertex_count = cppext::narrow<uint32_t>(p.vertices.size()),
                    .is_indexed = !p.indices.empty()};

                // clang-format off
                // cppcheck-suppress uselessAssignmentPtrArg
                vertices = std::ranges::transform(p.vertices, 
                    vertices,
                    to_gpu_vertex).out;
                // clang-format on

                if constexpr (std::is_same_v<uint32_t, unsigned int>)
                {
                    // cppcheck-suppress uselessAssignmentPtrArg
                    indices = std::ranges::copy(p.indices, indices).out;
                }
                else
                {
                    // clang-format off
                    // cppcheck-suppress uselessAssignmentPtrArg
                    indices = std::ranges::transform(p.indices,
                        indices,
                        cppext::narrow<uint32_t, unsigned int>).out;
                    // clang-format on
                }

                if (gp.is_indexed)
                {
                    gp.count = cppext::narrow<uint32_t>(p.indices.size());
                    gp.first = cppext::narrow<uint32_t>(running_index_count);
                    gp.vertex_offset =
                        cppext::narrow<int32_t>(running_vertex_count);
                }
                else
                {
                    gp.count = cppext::narrow<uint32_t>(p.vertices.size());
                    gp.vertex_offset =
                        cppext::narrow<int32_t>(running_vertex_count);
                    gp.first = cppext::narrow<uint32_t>(running_vertex_count);
                }

                [[maybe_unused]] bool overflow{cppext::add(running_index_count,
                    cppext::narrow<uint32_t>(p.indices.size()),
                    running_index_count)};
                assert(overflow);

                overflow = cppext::add(running_vertex_count,
                    cppext::narrow<int32_t>(p.vertices.size()),
                    running_vertex_count);
                assert(overflow);

                return gp;
            });

        return rv;
    }
} // namespace

void ngnast::gpu::destroy(vkrndr::device_t const& device,
    geometry_transfer_result_t const& model)
{
    destroy(device, model.vertex_buffer);
    destroy(device, model.index_buffer);
}

ngnast::gpu::geometry_transfer_result_t ngnast::gpu::transfer_geometry(
    vkrndr::device_t const& device,
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

                    [[maybe_unused]] bool overflow{cppext::add(vc,
                        cppext::narrow<uint32_t>(primitive.vertices.size()),
                        vc)};
                    assert(overflow);

                    overflow = cppext::add(ic,
                        cppext::narrow<uint32_t>(primitive.indices.size()),
                        ic);
                    assert(overflow);
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

    rv.primitives = transfer_primitives(model, vertices, indices);

    return rv;
}

void ngnast::gpu::destroy(vkrndr::device_t const& device,
    acceleration_structure_build_result_t const& structures)
{
    destroy(device, structures.vertex_buffer);
    destroy(device, structures.index_buffer);

    for (vkrndr::acceleration_structure_t const& structure :
        structures.bottom_level)
    {
        destroy(device, structure);
    }

    destroy(device, structures.top_level);
}

ngnast::gpu::acceleration_structure_build_result_t
ngnast::gpu::build_acceleration_structures(vkrndr::backend_t& backend,
    scene_model_t const& model)
{
    geometry_transfer_result_t intermediate{
        transfer_geometry(backend.device(), model)};
    [[maybe_unused]] boost::scope::defer_guard const destroy_transfer{
        [&backend, &intermediate]()
        { destroy(backend.device(), intermediate); }};

    acceleration_structure_build_result_t rv{
        .vertex_buffer = vkrndr::create_buffer(backend.device(),
            {
                .size = intermediate.vertex_buffer.size,
                .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .required_memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            }),
        .vertex_count = intermediate.vertex_count,
        .index_buffer = intermediate.index_count > 0
            ? vkrndr::create_buffer(backend.device(),
                  {
                      .size = intermediate.vertex_buffer.size,
                      .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                          VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                          VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                      .required_memory_flags =
                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                  })
            : vkrndr::buffer_t{},
        .index_count = intermediate.index_count,
        .primitives = std::move(intermediate.primitives)};
    [[maybe_unused]] boost::scope::scope_fail const destroy_rv{
        [&backend, &rv]() { destroy(backend.device(), rv); }};

    {
        vkrndr::transient_operation_t op{
            backend.request_transient_operation(false)};
        VkCommandBuffer cb{op.command_buffer()};

        std::vector<VkBufferMemoryBarrier2> transfer_geometry_barriers;

        VkBufferCopy const copy_vertex{
            .size = intermediate.vertex_buffer.size,
        };
        vkCmdCopyBuffer(cb,
            intermediate.vertex_buffer,
            rv.vertex_buffer,
            1,
            &copy_vertex);

        transfer_geometry_barriers.push_back(vkrndr::with_access(
            vkrndr::on_stage(vkrndr::buffer_barrier(rv.vertex_buffer),
                VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,
                VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR),
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_ACCESS_2_SHADER_READ_BIT));

        if (intermediate.index_count > 0)
        {
            VkBufferCopy const copy_index{
                .size = intermediate.index_buffer.size,
            };
            vkCmdCopyBuffer(cb,
                intermediate.index_buffer,
                rv.index_buffer,
                1,
                &copy_index);

            transfer_geometry_barriers.push_back(vkrndr::with_access(
                vkrndr::on_stage(vkrndr::buffer_barrier(rv.vertex_buffer),
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,
                    VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR),
                VK_ACCESS_2_TRANSFER_WRITE_BIT,
                VK_ACCESS_2_SHADER_READ_BIT));
        }

        vkrndr::wait_for(cb, {}, transfer_geometry_barriers, {});
    }

    std::vector<VkAccelerationStructureGeometryKHR> geometries;
    geometries.reserve(rv.primitives.size());

    std::vector<uint32_t> primitive_counts;
    primitive_counts.reserve(rv.primitives.size());

    std::vector<VkAccelerationStructureBuildGeometryInfoKHR> build_geometries;
    build_geometries.reserve(rv.primitives.size());

    std::vector<VkAccelerationStructureBuildRangeInfoKHR> build_range;
    build_range.reserve(rv.primitives.size());

    for (primitive_t const& primitive : rv.primitives)
    {
        assert(primitive.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

        primitive_counts.push_back(
            (primitive.is_indexed ? primitive.count : primitive.vertex_count) /
            3);

        geometries.push_back({
            .sType = vku::GetSType<VkAccelerationStructureGeometryKHR>(),
            .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
            .geometry =
                {.triangles =
                        {
                            .sType = vku::GetSType<
                                VkAccelerationStructureGeometryTrianglesDataKHR>(),
                            .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
                            .vertexData = {.deviceAddress =
                                               rv.vertex_buffer.device_address},
                            .vertexStride = sizeof(vertex_t),
                            .maxVertex = primitive_counts.back() - 1,
                        }},
            .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
        });

        build_range.push_back(VkAccelerationStructureBuildRangeInfoKHR{
            .primitiveCount = primitive_counts.back(),
            .firstVertex = static_cast<uint32_t>(primitive.vertex_offset),
        });

        if (primitive.is_indexed)
        {
            auto& geo{geometries.back().geometry.triangles};
            geo.indexType = VK_INDEX_TYPE_UINT32;
            geo.indexData = {.deviceAddress = rv.index_buffer.device_address};

            build_range.back().primitiveOffset =
                static_cast<uint32_t>(primitive.first * sizeof(uint32_t));
        }

        build_geometries.push_back({
            .sType =
                vku::GetSType<VkAccelerationStructureBuildGeometryInfoKHR>(),
            .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
            .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
            .geometryCount = 1,
            .pGeometries = &geometries.back(),
        });
    }

    std::vector<VkAccelerationStructureBuildRangeInfoKHR const*> build_ranges;
    build_ranges.reserve(rv.primitives.size());
    std::ranges::transform(build_range,
        std::back_inserter(build_ranges),
        [](VkAccelerationStructureBuildRangeInfoKHR const& r) { return &r; });

    std::vector<VkAccelerationStructureBuildSizesInfoKHR> build_sizes{
        rv.primitives.size(),
        vku::InitStruct<VkAccelerationStructureBuildSizesInfoKHR>()};

    VkDeviceSize scratch_size{};
    rv.bottom_level.reserve(rv.primitives.size());
    for (size_t i{}; i != rv.primitives.size(); ++i)
    {
        vkGetAccelerationStructureBuildSizesKHR(backend.device(),
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &build_geometries[i],
            &primitive_counts[i],
            &build_sizes[i]);

        rv.bottom_level.push_back(
            vkrndr::create_acceleration_structure(backend.device(),
                VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
                build_sizes[i]));

        [[maybe_unused]] bool const overflow{cppext::add(scratch_size,
            build_sizes[i].buildScratchSize,
            scratch_size)};
        assert(overflow);
    }

    vkrndr::buffer_t const scratch_buffer{
        vkrndr::create_scratch_buffer(backend.device(), scratch_size)};
    [[maybe_unused]] boost::scope::defer_guard const destroy_scratch{
        [&backend, &scratch_buffer]()
        { destroy(backend.device(), scratch_buffer); }};

    VkDeviceSize scratch_offset{};
    for (size_t i{}; i != rv.primitives.size(); ++i)
    {
        build_geometries[i].mode =
            VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        build_geometries[i].dstAccelerationStructure =
            rv.bottom_level[i].handle;
        build_geometries[i].scratchData.deviceAddress =
            scratch_buffer.device_address + scratch_offset;

        scratch_offset += build_sizes[i].buildScratchSize;
    }

    {
        vkrndr::transient_operation_t op{
            backend.request_transient_operation(false)};
        VkCommandBuffer cb{op.command_buffer()};

        vkCmdBuildAccelerationStructuresKHR(cb,
            rv.primitives.size(),
            build_geometries.data(),
            build_ranges.data());
    }
    // for (size_t i{}; i != rv.primitives.size() / 10; ++i)
    //{
    //     vkrndr::transient_operation_t op{
    //         backend.request_transient_operation(false)};
    //     VkCommandBuffer cb{op.command_buffer()};

    //    vkCmdBuildAccelerationStructuresKHR(cb,
    //        10,
    //        build_geometries.data() + i * 10,
    //        build_ranges.data() + i * 10);
    //}

    // if (auto const remainder{rv.primitives.size() % 10})
    //{
    //     vkrndr::transient_operation_t op{
    //         backend.request_transient_operation(false)};
    //     VkCommandBuffer cb{op.command_buffer()};

    //    vkCmdBuildAccelerationStructuresKHR(cb,
    //        remainder,
    //        build_geometries.data() + rv.primitives.size() - remainder,
    //        build_ranges.data() + rv.primitives.size() - remainder);
    //}

    return rv;
}
