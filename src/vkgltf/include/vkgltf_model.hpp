#ifndef VKGLTF_MODEL_INCLUDED
#define VKGLTF_MODEL_INCLUDED

#include <vkrndr_image.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <vulkan/vulkan_core.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace vkrndr
{
    struct device_t;
} // namespace vkrndr

namespace vkgltf
{
    struct [[nodiscard]] image_t final
    {
        std::string name;
        vkrndr::image_t source;
    };

    struct [[nodiscard]] texture_t final
    {
        std::string name;
        std::optional<size_t> sampler;
        std::optional<size_t> image;
    };

    struct [[nodiscard]] pbr_metallic_roughness_t final
    {
        glm::vec4 base_color_factor{1.0f};
        size_t texture;
    };

    struct [[nodiscard]] material_t final
    {
        std::string name;
        pbr_metallic_roughness_t pbr_metallic_roughness;
    };

    struct [[nodiscard]] index_buffer_t final
    {
        std::variant<std::vector<uint8_t>,
            std::vector<uint16_t>,
            std::vector<uint32_t>>
            buffer;

        [[nodiscard]] uint32_t count() const;
    };

    struct [[nodiscard]] primitive_t final
    {
        VkPrimitiveTopology topology{VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec4> tangents;
        std::vector<glm::vec2> uvs;
        std::optional<index_buffer_t> indices;
        std::optional<size_t> material;
    };

    struct [[nodiscard]] mesh_t final
    {
        std::string name;
        std::vector<primitive_t> primitives;
    };

    struct [[nodiscard]] node_t final
    {
        std::string name;
        std::optional<size_t> mesh;
        glm::mat4 matrix{1.0f};
        std::vector<size_t> children;
    };

    struct [[nodiscard]] scene_t final
    {
        std::string name;
        std::vector<size_t> nodes;
    };

    struct [[nodiscard]] model_t final
    {
        std::vector<scene_t> scenes;
        std::vector<node_t> nodes;
        std::vector<mesh_t> meshes;
        std::vector<material_t> materials;
        std::vector<texture_t> textures;
        std::vector<image_t> images;
        std::vector<VkSampler> samplers;
    };

    void destroy(vkrndr::device_t* device, model_t* model);
} // namespace vkgltf

#endif
