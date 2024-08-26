#ifndef VKRNDR_GLTF_MANAGER_INCLUDED
#define VKRNDR_GLTF_MANAGER_INCLUDED

#include <vkrndr_image.hpp>

#include <glm/fwd.hpp>
#include <glm/gtc/quaternion.hpp> // IWYU pragma: keep
#include <glm/mat4x4.hpp> // IWYU pragma: keep
#include <glm/vec3.hpp> // IWYU pragma: keep

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace vkrndr
{
    class backend_t;
} // namespace vkrndr

namespace vkrndr
{
    struct [[nodiscard]] gltf_bounding_box_t final
    {
        glm::fvec3 min;
        glm::fvec3 max;
    };

    gltf_bounding_box_t get_aabb(gltf_bounding_box_t const& box,
        glm::mat4 const& local_matrix);

    struct [[nodiscard]] gltf_vertex_t final
    {
        glm::fvec3 position;
        glm::fvec3 normal;
        glm::fvec2 texture_coordinate;
    };

    struct [[nodiscard]] gltf_texture_t final
    {
        image_t image;
    };

    struct [[nodiscard]] gltf_material_t final
    {
        uint32_t index{};
        gltf_texture_t* base_color_texture{};
        uint8_t base_color_coord_set{};
    };

    struct [[nodiscard]] gltf_primitive_t final
    {
        std::vector<gltf_vertex_t> vertices;
        std::vector<uint32_t> indices;
        std::optional<gltf_bounding_box_t> bounding_box;
        gltf_material_t* material{};
    };

    struct [[nodiscard]] gltf_mesh_t final
    {
        std::string name;
        std::vector<gltf_primitive_t> primitives;
        std::optional<gltf_bounding_box_t> bounding_box;
    };

    struct [[nodiscard]] gltf_node_t final
    {
        std::string name;
        std::optional<gltf_mesh_t> mesh;
        glm::fvec3 translation{0.0f};
        glm::fvec3 scale{1.0f};
        glm::fquat rotation{};
        glm::fmat4 matrix{1.0f};
        std::optional<gltf_bounding_box_t> bounding_box;
        std::optional<gltf_bounding_box_t> axis_aligned_bounding_box;
    };

    struct [[nodiscard]] gltf_model_t final
    {
        std::vector<gltf_node_t> nodes;
        std::vector<gltf_material_t> materials;
        std::vector<gltf_texture_t> textures;
    };

    glm::fmat4 local_matrix(gltf_node_t const& node);

    class [[nodiscard]] gltf_manager_t final
    {
    public:
        explicit gltf_manager_t(backend_t* backend);

        gltf_manager_t(gltf_manager_t const&) = delete;

        gltf_manager_t(gltf_manager_t&&) noexcept = default;

    public:
        ~gltf_manager_t() = default;

    public:
        std::unique_ptr<gltf_model_t> load(std::filesystem::path const& path);

    public:
        gltf_manager_t& operator=(gltf_manager_t const&) = delete;

        gltf_manager_t& operator=(gltf_manager_t&&) noexcept = default;

    private:
        backend_t* backend_;
    };
} // namespace vkrndr

#endif
