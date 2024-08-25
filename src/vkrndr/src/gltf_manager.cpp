#include <gltf_manager.hpp>

#include <vulkan_image.hpp>
#include <vulkan_renderer.hpp>
#include <vulkan_utility.hpp>

#include <cppext_numeric.hpp>
#include <cppext_pragma_warning.hpp>

#include <tinygltf_impl.hpp>

#include <fmt/format.h>
#include <fmt/std.h> // IWYU pragma: keep

#include <glm/common.hpp>
#include <glm/fwd.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

// IWYU pragma: no_include <map>
// IWYU pragma: no_include <glm/detail/qualifier.hpp>

namespace
{
    template<typename T>
    [[nodiscard]] constexpr size_t size_cast(T const value)
    {
        return cppext::narrow<size_t>(value);
    }

    // NOLINTBEGIN(cppcoreguidelines-avoid-const-or-ref-data-members)
    struct [[nodiscard]] primitive_data final
    {
        tinygltf::Accessor const& accessor;
        tinygltf::Buffer const& buffer;
        tinygltf::BufferView const& view;
    };

    // NOLINTEND(cppcoreguidelines-avoid-const-or-ref-data-members)

    constexpr primitive_data data_for(tinygltf::Model const& model,
        int const accessor_index)
    {
        tinygltf::Accessor const& accessor{
            model.accessors[size_cast(accessor_index)]};

        tinygltf::BufferView const& view{
            model.bufferViews[size_cast(accessor.bufferView)]};

        tinygltf::Buffer const& buffer{model.buffers[size_cast(view.buffer)]};

        return {accessor, buffer, view};
    }

    template<typename TargetType>
    struct [[nodiscard]] primitive_buffer final
    {
        TargetType const* ptr{};
        size_t stride{};
        size_t count{};

        constexpr operator std::tuple<TargetType const*&, size_t&, size_t&>()
        {
            return {ptr, stride, count};
        }
    };

    template<typename TargetType>
    constexpr primitive_buffer<TargetType> buffer_for(
        tinygltf::Model const& model,
        int const accessor_index,
        uint32_t type = TINYGLTF_TYPE_VEC3)
    {
        auto const& [accessor, buffer, view] = data_for(model, accessor_index);

        primitive_buffer<TargetType> rv;

        // NOLINTNEXTLINE
        rv.ptr = reinterpret_cast<TargetType const*>(
            &(buffer.data[accessor.byteOffset + view.byteOffset]));

        size_t const byte_stride{size_cast(accessor.ByteStride(view))};

        rv.stride = byte_stride
            ? byte_stride / sizeof(float)
            : size_cast(tinygltf::GetNumComponentsInType(type));

        rv.count = accessor.count;

        return rv;
    }

    std::vector<vkrndr::gltf_vertex> load_vertices(tinygltf::Model const& model,
        tinygltf::Primitive const& primitive)
    {
        std::vector<vkrndr::gltf_vertex> rv;

        float const* position_buffer{nullptr};
        float const* normal_buffer{nullptr};
        float const* texture_buffer{nullptr};

        size_t vertex_count{};

        size_t position_stride{};
        size_t normal_stride{};
        size_t texture_stride{};

        // Get buffer data for vertex normals
        if (auto const it{primitive.attributes.find("POSITION")};
            it != primitive.attributes.end())
        {
            std::tie(position_buffer, position_stride, vertex_count) =
                buffer_for<float>(model, it->second);
        }

        if (auto const it{primitive.attributes.find("NORMAL")};
            it != primitive.attributes.end())
        {
            size_t normal_count; // NOLINT
            std::tie(normal_buffer, normal_stride, normal_count) =
                buffer_for<float>(model, it->second);
        }

        if (auto const it{primitive.attributes.find("TEXCOORD_0")};
            it != primitive.attributes.end())
        {
            size_t texcoord_count; // NOLINT
            std::tie(texture_buffer, texture_stride, texcoord_count) =
                buffer_for<float>(model, it->second, TINYGLTF_TYPE_VEC2);
        }

        for (size_t vertex{}; vertex != vertex_count; ++vertex)
        {
            vkrndr::gltf_vertex const vert{
                .position =
                    glm::make_vec3(&position_buffer[vertex * position_stride]),
                .normal = glm::normalize(normal_buffer
                        ? glm::make_vec3(&normal_buffer[vertex * normal_stride])
                        : glm::fvec3(0.0f)),
                .texture_coordinate = texture_buffer
                    ? glm::make_vec2(&texture_buffer[vertex * texture_stride])
                    : glm::fvec2(0.0f)};

            static_assert(std::is_trivial_v<vkrndr::gltf_vertex>);
            rv.push_back(vert);
        }
        return rv;
    }

    std::vector<uint32_t> load_indices(tinygltf::Model const& model,
        tinygltf::Primitive const& primitive)
    {
        auto const& [accessor, buffer, view] =
            data_for(model, primitive.indices);

        std::vector<uint32_t> rv;
        rv.reserve(accessor.count);

        // glTF supports different component types of indices
        switch (accessor.componentType)
        {
        case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT:
        {
            // NOLINTNEXTLINE
            auto const* const buf{reinterpret_cast<uint32_t const*>(
                &buffer.data[accessor.byteOffset + view.byteOffset])};
            rv.insert(std::end(rv), buf, buf + accessor.count);
            break;
        }
        case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT:
        {
            // NOLINTNEXTLINE
            auto const* const buf{reinterpret_cast<uint16_t const*>(
                &buffer.data[accessor.byteOffset + view.byteOffset])};
            std::ranges::copy(buf,
                buf + accessor.count,
                std::back_inserter(rv));
            break;
        }
        case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE:
        {
            DISABLE_WARNING_PUSH
            DISABLE_WARNING_USELESS_CAST
            // NOLINTNEXTLINE
            auto const* const buf{reinterpret_cast<uint8_t const*>(
                &buffer.data[accessor.byteOffset + view.byteOffset])};
            DISABLE_WARNING_POP

            std::ranges::copy(buf,
                buf + accessor.count,
                std::back_inserter(rv));
            break;
        }
        default:
            throw std::runtime_error{
                fmt::format("Index component type {} not supported",
                    accessor.componentType)};
        }

        return rv;
    }

    std::optional<vkrndr::gltf_bounding_box> load_bounding_box(
        tinygltf::Model const& model,
        tinygltf::Primitive const& primitive)
    {
        if (auto const it{primitive.attributes.find("POSITION")};
            it != primitive.attributes.end())
        {
            auto const& [accessor, buffer, view] = data_for(model, it->second);
            auto const& min{accessor.minValues};
            auto const& max{accessor.maxValues};
            return vkrndr::gltf_bounding_box{glm::fvec3{min[0], min[1], min[2]},
                glm::fvec3{max[0], max[1], max[2]}};
        }
        return std::nullopt;
    }

    void load_transform(tinygltf::Node const& node, vkrndr::gltf_node& new_node)
    {
        if (node.translation.size() == 3)
        {
            new_node.translation = glm::make_vec3(node.translation.data());
        }

        if (node.rotation.size() == 4)
        {
            glm::fquat const quaternion{glm::make_quat(node.rotation.data())};
            new_node.rotation = glm::mat4{quaternion};
        }

        if (node.scale.size() == 3)
        {
            new_node.scale = glm::make_vec3(node.scale.data());
        }

        if (node.matrix.size() == 16)
        {
            new_node.matrix = glm::make_mat4x4(node.matrix.data());
        }
    }

    void load_textures(vkrndr::vulkan_renderer* renderer,
        tinygltf::Model& model,
        vkrndr::gltf_model& new_model)
    {
        for (tinygltf::Texture const& tex : model.textures)
        {
            auto const source{size_cast(tex.source)};
            tinygltf::Image const& image{model.images[source]};

            vkrndr::vulkan_image const texture_image{
                renderer->transfer_image(vkrndr::as_bytes(image.image),
                    {cppext::narrow<uint32_t>(image.width),
                        cppext::narrow<uint32_t>(image.height)},
                    VK_FORMAT_R8G8B8A8_SRGB,
                    vkrndr::max_mip_levels(image.width, image.height))};

            new_model.textures.emplace_back(texture_image);
        }
    }

    void load_materials(tinygltf::Model const& model,
        vkrndr::gltf_model& new_model)
    {
        for (tinygltf::Material const& material : model.materials)
        {
            vkrndr::gltf_material new_material;

            tinygltf::TextureInfo const& texture{
                material.pbrMetallicRoughness.baseColorTexture};

            new_material.base_color_texture =
                &new_model.textures[size_cast(texture.index)];
            new_material.base_color_coord_set =
                cppext::narrow<uint8_t>(texture.texCoord);

            new_material.index =
                cppext::narrow<uint32_t>(new_model.materials.size());
            new_model.materials.push_back(new_material);
        }

        // Push a default material at the end of the list for meshes with no
        // material assigned
        new_model.materials.emplace_back();
    }
} // namespace

vkrndr::gltf_manager::gltf_manager(vulkan_renderer* renderer)
    : renderer_{renderer}
{
}

std::unique_ptr<vkrndr::gltf_model> vkrndr::gltf_manager::load(
    std::filesystem::path const& path)
{
    tinygltf::TinyGLTF context;

    tinygltf::Model model;
    std::string error;
    std::string warning;
    bool const loaded{path.extension() == ".gltf"
            ? context.LoadASCIIFromFile(&model, &error, &warning, path.string())
            : context.LoadBinaryFromFile(&model,
                  &error,
                  &warning,
                  path.string())};
    if (!loaded)
    {
        throw std::runtime_error{
            fmt::format("Model at path {} not loaded", path)};
    }

    auto rv{std::make_unique<gltf_model>()};

    load_textures(renderer_, model, *rv);
    load_materials(model, *rv);

    for (tinygltf::Node const& node : model.nodes)
    {
        DISABLE_WARNING_PUSH
        DISABLE_WARNING_MISSING_FIELD_INITIALIZERS
        gltf_node new_node{.name = node.name};
        DISABLE_WARNING_POP

        load_transform(node, new_node);

        if (node.mesh != -1)
        {
            tinygltf::Mesh const& mesh{model.meshes[size_cast(node.mesh)]};

            gltf_mesh new_mesh;
            new_mesh.name = mesh.name;

            auto& mesh_bounding_box{new_mesh.bounding_box};

            for (tinygltf::Primitive const& primitive : mesh.primitives)
            {
                gltf_primitive new_primitive{
                    .vertices = load_vertices(model, primitive),
                    .indices = load_indices(model, primitive),
                    .bounding_box = load_bounding_box(model, primitive),
                    .material = primitive.material >= 0
                        ? &rv->materials[size_cast(primitive.material)]
                        : &rv->materials.back()};

                if (new_primitive.bounding_box)
                {
                    if (!mesh_bounding_box)
                    {
                        mesh_bounding_box = new_primitive.bounding_box;
                    }
                    else
                    {
                        mesh_bounding_box->min =
                            glm::min(new_primitive.bounding_box->min,
                                mesh_bounding_box->min);
                        mesh_bounding_box->max =
                            glm::max(new_primitive.bounding_box->max,
                                mesh_bounding_box->max);
                    }
                }

                new_mesh.primitives.push_back(std::move(new_primitive));
            }
            new_node.mesh = std::move(new_mesh);
            if (new_node.mesh->bounding_box)
            {
                if (!new_node.bounding_box)
                {
                    new_node.bounding_box = new_node.mesh->bounding_box;
                }
                else
                {
                    new_node.bounding_box->min =
                        glm::min(new_node.bounding_box->min,
                            new_node.mesh->bounding_box->min);
                    new_node.bounding_box->max =
                        glm::max(new_node.bounding_box->max,
                            new_node.mesh->bounding_box->max);
                }
            }
        }

        if (new_node.mesh && new_node.mesh->bounding_box)
        {
            new_node.axis_aligned_bounding_box =
                get_aabb(*new_node.mesh->bounding_box, local_matrix(new_node));
        }

        rv->nodes.push_back(std::move(new_node));
    }
    return rv;
}

vkrndr::gltf_bounding_box vkrndr::get_aabb(gltf_bounding_box const& box,
    glm::fmat4 const& local_matrix)
{
    glm::fvec3 min{glm::vec3{local_matrix[3]}};
    glm::fvec3 max{min};

    glm::fvec3 const right{glm::vec3{local_matrix[0]}};
    glm::fvec3 v0{right * box.min.x};
    glm::fvec3 v1{right * box.max.x};
    min += glm::min(v0, v1);
    max += glm::max(v0, v1);

    glm::vec3 const up{glm::vec3{local_matrix[1]}};
    v0 = up * box.min.y;
    v1 = up * box.max.y;
    min += glm::min(v0, v1);
    max += glm::max(v0, v1);

    glm::vec3 const back{glm::vec3{local_matrix[2]}};
    v0 = back * box.min.z;
    v1 = back * box.max.z;
    min += glm::min(v0, v1);
    max += glm::max(v0, v1);

    return {min, max};
}

glm::fmat4 vkrndr::local_matrix(gltf_node const& node)
{
    return glm::translate(glm::mat4(1.0f), node.translation) *
        glm::mat4(node.rotation) * glm::scale(glm::mat4(1.0f), node.scale) *
        node.matrix;
}
