#ifndef NGNAST_GPU_TRANSFER_INCLUDED
#define NGNAST_GPU_TRANSFER_INCLUDED

#include <vkrndr_acceleration_structure.hpp>
#include <vkrndr_buffer.hpp>
#include <vkrndr_image.hpp>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <volk.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ngnast
{
    struct scene_model_t;
    struct image_mip_level_t;
} // namespace ngnast

namespace vkrndr
{
    struct device_t;
    class backend_t;
} // namespace vkrndr

namespace ngnast::gpu
{
    struct [[nodiscard]] vertex_t final
    {
        glm::vec3 position;
        float uv_s;
        glm::vec3 normal;
        float uv_t;
        glm::vec4 tangent;
        glm::vec4 color{1.0f};
    };

    struct [[nodiscard]] primitive_t final
    {
        VkPrimitiveTopology topology{VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
        size_t material_index{};

        uint32_t vertex_count{};
        uint32_t count{};
        uint32_t first{};

        bool is_indexed{};
        int32_t vertex_offset{};
    };

    struct [[nodiscard]] geometry_transfer_result_t final
    {
        vkrndr::buffer_t vertex_buffer;
        uint32_t vertex_count{};

        vkrndr::buffer_t index_buffer;
        uint32_t index_count{};

        std::vector<primitive_t> primitives;
    };

    void destroy(vkrndr::device_t const& device,
        geometry_transfer_result_t const& model);

    struct [[nodiscard]] acceleration_structure_build_result_t final
    {
        vkrndr::buffer_t vertex_buffer;
        uint32_t vertex_count{};

        vkrndr::buffer_t index_buffer;
        uint32_t index_count{};

        std::vector<primitive_t> primitives;

        std::vector<vkrndr::acceleration_structure_t> bottom_level;

        vkrndr::buffer_t instance_buffer;

        vkrndr::acceleration_structure_t top_level;
    };

    void destroy(vkrndr::device_t const& device,
        acceleration_structure_build_result_t const& structures);

    geometry_transfer_result_t transfer_geometry(vkrndr::device_t const& device,
        scene_model_t const& model);

    acceleration_structure_build_result_t build_acceleration_structures(
        vkrndr::backend_t& backend,
        scene_model_t const& model);

    vkrndr::image_mip_level_t to_vulkan(image_mip_level_t const& level);
} // namespace ngnast::gpu

#endif
