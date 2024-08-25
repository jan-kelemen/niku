#ifndef VKRNDR_GLTF_MANAGER_INCLUDED
#define VKRNDR_GLTF_MANAGER_INCLUDED

#include <vulkan_image.hpp>

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
    class vulkan_renderer;
} // namespace vkrndr

namespace vkrndr
{
    struct [[nodiscard]] gltf_bounding_box final
    {
        glm::fvec3 min;
        glm::fvec3 max;
    };

    gltf_bounding_box get_aabb(gltf_bounding_box const& box,
        glm::mat4 const& local_matrix);

    struct [[nodiscard]] gltf_vertex final
    {
        glm::fvec3 position;
        glm::fvec3 normal;
        glm::fvec2 texture_coordinate;
    };

    struct [[nodiscard]] gltf_texture final
    {
        vulkan_image image;
    };

    struct [[nodiscard]] gltf_material final
    {
        uint32_t index{};
        gltf_texture* base_color_texture{};
        uint8_t base_color_coord_set{};
    };

    struct [[nodiscard]] gltf_primitive final
    {
        std::vector<gltf_vertex> vertices;
        std::vector<uint32_t> indices;
        std::optional<gltf_bounding_box> bounding_box;
        gltf_material* material{};
    };

    struct [[nodiscard]] gltf_mesh final
    {
        std::string name;
        std::vector<gltf_primitive> primitives;
        std::optional<gltf_bounding_box> bounding_box;
    };

    struct [[nodiscard]] gltf_node final
    {
        std::string name;
        std::optional<gltf_mesh> mesh;
        glm::fvec3 translation{0.0f};
        glm::fvec3 scale{1.0f};
        glm::fquat rotation{};
        glm::fmat4 matrix{1.0f};
        std::optional<gltf_bounding_box> bounding_box;
        std::optional<gltf_bounding_box> axis_aligned_bounding_box;
    };

    struct [[nodiscard]] gltf_model final
    {
        std::vector<gltf_node> nodes;
        std::vector<gltf_material> materials;
        std::vector<gltf_texture> textures;
    };

    glm::fmat4 local_matrix(gltf_node const& node);

    class [[nodiscard]] gltf_manager final
    {
    public:
        explicit gltf_manager(vulkan_renderer* renderer);

        gltf_manager(gltf_manager const&) = delete;

        gltf_manager(gltf_manager&&) noexcept = default;

    public:
        ~gltf_manager() = default;

    public:
        std::unique_ptr<gltf_model> load(std::filesystem::path const& path);

    public:
        gltf_manager& operator=(gltf_manager const&) = delete;

        gltf_manager& operator=(gltf_manager&&) noexcept = default;

    private:
        vulkan_renderer* renderer_;
    };
} // namespace vkrndr

#endif
