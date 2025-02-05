#ifndef NGNAST_GPU_TRANSFER_INCLUDED
#define NGNAST_GPU_TRANSFER_INCLUDED

#include <vkrndr_buffer.hpp>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <volk.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ngnast
{
    struct scene_model_t;
} // namespace ngnast

namespace vkrndr
{
    struct device_t;
} // namespace vkrndr

namespace ngnast::gpu
{
    struct [[nodiscard]] vertex_t final
    {
        glm::vec3 position;
        uint8_t padding1[4];
        glm::vec3 normal;
        uint8_t padding2[4];
        glm::vec4 tangent;
        glm::vec4 color{1.0f};
        glm::vec2 uv;
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

    void destroy(vkrndr::device_t* device, geometry_transfer_result_t* model);

    geometry_transfer_result_t transfer_geometry(vkrndr::device_t& device,
        scene_model_t const& model);
} // namespace ngnast::gpu

#endif
