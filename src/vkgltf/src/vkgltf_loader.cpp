#include <vkgltf_loader.hpp>

#include <vkgltf_error.hpp>
#include <vkgltf_fastgltf_adapter.hpp>
#include <vkgltf_model.hpp>

#include <cppext_numeric.hpp>
#include <cppext_overloaded.hpp>
#include <cppext_pragma_warning.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_buffer.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_memory.hpp>
#include <vkrndr_utility.hpp>

#include <boost/scope/defer.hpp>
#include <boost/scope/scope_exit.hpp>

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp> // IWYU pragma: keep
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <meshoptimizer.h>

#include <mikktspace.h>

#include <spdlog/spdlog.h>

#include <stb_image.h>

#include <volk.h>

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <exception> // IWYU pragma: keep
#include <expected>
#include <iterator>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <variant>
#include <vector>

// IWYU pragma: no_include <fastgltf/math.hpp>
// IWYU pragma: no_include <fastgltf/util.hpp>
// IWYU pragma: no_include <fmt/base.h>
// IWYU pragma: no_include <fmt/format.h>
// IWYU pragma: no_include <glm/detail/qualifier.hpp>
// IWYU pragma: no_include <cmath>
// IWYU pragma: no_include <format>
// IWYU pragma: no_include <functional>

namespace
{
    constexpr auto position_attribute{"POSITION"};
    constexpr auto normal_attribute{"NORMAL"};
    constexpr auto tangent_attribute{"TANGENT"};
    constexpr auto texcoord_0_attribute{"TEXCOORD_0"};
    constexpr auto color_attribute{"COLOR_0"};

    struct [[nodiscard]] loaded_vertex_t final
    {
        float position[3]{};
        glm::vec3 normal{};
        glm::vec4 tangent{};
        glm::vec4 color{1.0f};
        glm::vec2 uv{};
    };

    struct [[nodiscard]] loaded_primitive_t final
    {
        VkPrimitiveTopology topology{VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

        std::vector<loaded_vertex_t> vertices;
        std::vector<unsigned int> indices;

        size_t material_index{};
    };

    [[nodiscard]] bool make_unindexed(loaded_primitive_t& primitive)
    {
        if (primitive.indices.empty())
        {
            return false;
        }

        std::vector<loaded_vertex_t> new_vertices;
        new_vertices.reserve(primitive.indices.size());
        std::ranges::transform(primitive.indices,
            std::back_inserter(new_vertices),
            [&primitive](unsigned int const i)
            { return primitive.vertices[i]; });

        primitive.vertices = std::move(new_vertices);
        primitive.indices.clear();

        return true;
    }

    void make_indexed(loaded_primitive_t& primitive)
    {
        size_t const index_count{primitive.vertices.size()};
        size_t const unindexed_vertex_count{primitive.vertices.size()};

        std::vector<unsigned int> remap;
        remap.resize(index_count);

        size_t const vertex_count{meshopt_generateVertexRemap(remap.data(),
            nullptr,
            index_count,
            primitive.vertices.data(),
            unindexed_vertex_count,
            sizeof(loaded_vertex_t))};

        std::vector<loaded_vertex_t> new_vertices;
        new_vertices.resize(vertex_count);
        meshopt_remapVertexBuffer(new_vertices.data(),
            primitive.vertices.data(),
            unindexed_vertex_count,
            sizeof(loaded_vertex_t),
            remap.data());
        primitive.vertices = std::move(new_vertices);

        primitive.indices.resize(index_count);
        meshopt_remapIndexBuffer(primitive.indices.data(),
            nullptr,
            index_count,
            remap.data());
    }

    struct [[nodiscard]] loaded_mesh_t final
    {
        std::string name;
        std::vector<loaded_primitive_t> primitives;

        bool consumed{false};
    };

    void calculate_normals(loaded_primitive_t& primitive)
    {
        bool const is_indexed{!primitive.indices.empty()};

        auto& vertices{primitive.vertices};
        auto const& indices{primitive.indices};

        size_t const vertex_count{
            is_indexed ? primitive.indices.size() : primitive.vertices.size()};
        for (uint32_t i{}; i != vertex_count; i += 3)
        {
            auto& point1{is_indexed ? vertices[indices[i]] : vertices[i]};
            auto& point2{
                is_indexed ? vertices[indices[i + 1]] : vertices[i + 1]};
            auto& point3{
                is_indexed ? vertices[indices[i + 2]] : vertices[i + 2]};

            // NOLINTBEGIN(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
            glm::vec3 const edge1{glm::make_vec3(point2.position) -
                glm::make_vec3(point1.position)};
            glm::vec3 const edge2{glm::make_vec3(point3.position) -
                glm::make_vec3(point1.position)};
            // NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay)

            // https://stackoverflow.com/a/57812028
            glm::vec3 const face_normal{
                glm::normalize(glm::cross(edge1, edge2))};
            point1.normal = face_normal;
            point2.normal = face_normal;
            point3.normal = face_normal;
        }

        for (auto& vertex : primitive.vertices)
        {
            vertex.normal = glm::normalize(vertex.normal);
        }
    }

    [[nodiscard]] bool calculate_tangents(loaded_primitive_t& primitive)
    {
        auto const get_num_faces = [](SMikkTSpaceContext const* context) -> int
        {
            auto* const v{static_cast<std::vector<loaded_vertex_t>*>(
                context->m_pUserData)};

            return cppext::narrow<int>(v->size() / 3);
        };

        auto const get_num_vertices_of_face =
            []([[maybe_unused]] SMikkTSpaceContext const* context,
                [[maybe_unused]] int const face) -> int { return 3; };

        auto const get_position = [](SMikkTSpaceContext const* context,
                                      float* const out,
                                      int const face,
                                      int const vertex)
        {
            auto const& v{*static_cast<std::vector<loaded_vertex_t>*>(
                context->m_pUserData)};

            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
            std::copy_n(v[cppext::narrow<size_t>(face * 3 + vertex)].position,
                3,
                out);
        };

        auto const get_normal = [](SMikkTSpaceContext const* context,
                                    float* const out,
                                    int const face,
                                    int const vertex)
        {
            auto const& v{*static_cast<std::vector<loaded_vertex_t>*>(
                context->m_pUserData)};

            std::copy_n(
                glm::value_ptr(
                    v[cppext::narrow<size_t>(face * 3 + vertex)].normal),
                3,
                out);
        };

        auto const get_uv = [](SMikkTSpaceContext const* context,
                                float* const out,
                                int const face,
                                int const vertex)
        {
            auto const& v{*static_cast<std::vector<loaded_vertex_t>*>(
                context->m_pUserData)};

            std::copy_n(
                glm::value_ptr(v[cppext::narrow<size_t>(face * 3 + vertex)].uv),
                2,
                out);
        };

        auto const set_tangent = [](SMikkTSpaceContext const* context,
                                     float const* const tangent,
                                     float const sign,
                                     int const face,
                                     int const vertex)
        {
            auto& v{*static_cast<std::vector<loaded_vertex_t>*>(
                context->m_pUserData)};

            v[cppext::narrow<size_t>(face * 3 + vertex)].tangent =
                glm::vec4{tangent[0], tangent[1], tangent[2], -sign};
        };

        SMikkTSpaceInterface interface{.m_getNumFaces = get_num_faces,
            .m_getNumVerticesOfFace = get_num_vertices_of_face,
            .m_getPosition = get_position,
            .m_getNormal = get_normal,
            .m_getTexCoord = get_uv,
            .m_setTSpaceBasic = set_tangent,
            .m_setTSpace = nullptr};

        SMikkTSpaceContext const context{.m_pInterface = &interface,
            .m_pUserData = &primitive.vertices};

        return static_cast<bool>(genTangSpaceDefault(&context));
    }

    template<typename T>
    [[nodiscard]] std::optional<uint32_t> copy_attribute(
        fastgltf::Asset const& asset,
        fastgltf::Primitive const& primitive,
        std::string_view attribute_name,
        auto&& transform)
    {
        if (auto const* const attr{primitive.findAttribute(attribute_name)};
            attr != primitive.attributes.cend())
        {
            auto const& accessor{asset.accessors[attr->accessorIndex]};
            if (!accessor.bufferViewIndex.has_value())
            {
                return std::nullopt;
            }

            fastgltf::iterateAccessorWithIndex<T>(asset, accessor, transform);

            return cppext::narrow<uint32_t>(accessor.count);
        }

        return std::nullopt;
    }

    [[nodiscard]] std::optional<uint32_t> copy_color_attribute(
        fastgltf::Asset const& asset,
        fastgltf::Primitive const& primitive,
        auto&& transform)
    {
        if (auto const* const attr{primitive.findAttribute(color_attribute)};
            attr != primitive.attributes.cend())
        {
            auto const& accessor{asset.accessors[attr->accessorIndex]};
            if (!accessor.bufferViewIndex.has_value())
            {
                return std::nullopt;
            }

            if (accessor.type == fastgltf::AccessorType::Vec3)
            {
                fastgltf::iterateAccessorWithIndex<glm::vec3>(asset,
                    accessor,
                    transform);
            }
            else if (accessor.type == fastgltf::AccessorType::Vec4)
            {
                fastgltf::iterateAccessorWithIndex<glm::vec4>(asset,
                    accessor,
                    transform);
            }
            else
            {
                assert(false);
            }

            return cppext::narrow<uint32_t>(accessor.count);
        }

        return std::nullopt;
    }

    void load_indices(fastgltf::Asset const& asset,
        fastgltf::Primitive const& primitive,
        std::vector<unsigned int>& indices)
    {
        if (!primitive.indicesAccessor.has_value())
        {
            return;
        }

        auto const& accessor{asset.accessors[*primitive.indicesAccessor]};
        indices.resize(accessor.count);

        fastgltf::copyFromAccessor<unsigned int>(asset,
            accessor,
            indices.data());
    }

    [[nodiscard]] std::expected<vkrndr::image_t, std::error_code> load_image(
        vkrndr::backend_t* const backend,
        std::filesystem::path const& parent_path,
        bool const as_unorm,
        fastgltf::Asset const& asset,
        fastgltf::Image const& image)
    {
        auto const transfer_image = [backend, as_unorm](int const width,
                                        int const height,
                                        stbi_uc* const data)
            -> std::expected<vkrndr::image_t, std::error_code>
        {
            if (data)
            {
                boost::scope::defer_guard const free_image{
                    [&data] { stbi_image_free(data); }};

                auto const extent{vkrndr::to_extent(width, height)};
                std::span<std::byte const> const image_data{
                    std::bit_cast<std::byte const*>(data),
                    size_t{extent.width} * extent.height * 4};

                return backend->transfer_image(image_data,
                    extent,
                    as_unorm ? VK_FORMAT_R8G8B8A8_UNORM
                             : VK_FORMAT_R8G8B8A8_SRGB,
                    vkrndr::max_mip_levels(width, height));
            }

            return std::unexpected{
                make_error_code(vkgltf::error_t::invalid_file)};
        };

        auto const load_from_vector =
            [&](fastgltf::sources::Vector const& vector)
        {
            int width; // NOLINT
            int height; // NOLINT
            int channels; // NOLINT
            stbi_uc* const data{stbi_load_from_memory(
                std::bit_cast<stbi_uc const*>(vector.bytes.data()),
                cppext::narrow<int>(vector.bytes.size()),
                &width,
                &height,
                &channels,
                4)};
            return transfer_image(width, height, data);
        };

        auto const load_from_array = [&](fastgltf::sources::Array const& vector)
        {
            int width; // NOLINT
            int height; // NOLINT
            int channels; // NOLINT
            stbi_uc* const data{stbi_load_from_memory(
                std::bit_cast<stbi_uc const*>(vector.bytes.data()),
                cppext::narrow<int>(vector.bytes.size()),
                &width,
                &height,
                &channels,
                4)};
            return transfer_image(width, height, data);
        };

        auto const load_from_uri = [&transfer_image, &parent_path](
                                       fastgltf::sources::URI const& filePath)
            -> std::expected<vkrndr::image_t, std::error_code>
        {
            // No offsets in file
            assert(filePath.fileByteOffset == 0);
            // Only local files
            assert(filePath.uri.isLocalPath());

            std::filesystem::path path{filePath.uri.fspath()};
            if (path.is_relative())
            {
                path = parent_path / path;
            }

            int width; // NOLINT
            int height; // NOLINT
            int channels; // NOLINT
            stbi_uc* const data{stbi_load(path.string().c_str(),
                &width,
                &height,
                &channels,
                4)};
            if (data == nullptr)
            {
                return std::unexpected{
                    make_error_code(vkgltf::error_t::unknown)};
            }

            return transfer_image(width, height, data);
        };

        auto const unsupported_variant = []([[maybe_unused]] auto&& arg)
            -> std::expected<vkrndr::image_t, std::error_code>
        {
            spdlog::error("Unsupported variant '{}' during image load",
                typeid(arg).name());
            return std::unexpected{make_error_code(vkgltf::error_t::unknown)};
        };

        auto const load_from_buffer_view =
            [&unsupported_variant, &load_from_array, &load_from_vector, &asset](
                fastgltf::sources::BufferView const& view)
        {
            auto const& bufferView = asset.bufferViews[view.bufferViewIndex];
            auto const& buffer = asset.buffers[bufferView.bufferIndex];

            return std::visit(cppext::overloaded{unsupported_variant,
                                  load_from_array,
                                  load_from_vector},
                buffer.data);
        };

        return std::visit(cppext::overloaded{load_from_uri,
                              load_from_array,
                              load_from_vector,
                              load_from_buffer_view,
                              unsupported_variant},
            image.data);
    }

    void load_images(vkrndr::backend_t* const backend,
        std::filesystem::path const& parent_path,
        std::set<size_t> const& unorm_images,
        fastgltf::Asset const& asset,
        vkgltf::model_t& model)
    {
        for (size_t i{}; i != asset.images.size(); ++i)
        {
            auto const& image{asset.images[i]};

            auto load_result{load_image(backend,
                parent_path,
                unorm_images.contains(i),
                asset,
                image)};
            if (!load_result)
            {
                throw std::system_error{load_result.error()};
            }

            model.images.push_back(load_result.value());
        }
    }

    void load_samplers(fastgltf::Asset const& asset, vkgltf::model_t& model)
    {
        model.samplers.reserve(asset.samplers.size() + 1);
        for (fastgltf::Sampler const& sampler : asset.samplers)
        {
            model.samplers.emplace_back(
                vkgltf::to_vulkan(
                    sampler.magFilter.value_or(fastgltf::Filter::Nearest)),
                vkgltf::to_vulkan(
                    sampler.minFilter.value_or(fastgltf::Filter::Nearest)),
                vkgltf::to_vulkan(sampler.wrapS),
                vkgltf::to_vulkan(sampler.wrapT),
                vkgltf::to_vulkan_mipmap(
                    sampler.minFilter.value_or(fastgltf::Filter::Nearest)));
        }

        model.samplers.emplace_back(); // Add default sampler to the end
    }

    DISABLE_WARNING_PUSH
    DISABLE_WARNING_MISSING_FIELD_INITIALIZERS

    void load_textures(fastgltf::Asset const& asset, vkgltf::model_t& model)
    {
        for (fastgltf::Texture const& texture : asset.textures)
        {
            vkgltf::texture_t t{.name = std::string{texture.name}};

            assert(texture.imageIndex);
            t.image_index = *texture.imageIndex;

            t.sampler_index =
                texture.samplerIndex.value_or(model.samplers.size() - 1);

            model.textures.push_back(std::move(t));
        }
    }

    [[nodiscard]] std::set<size_t> load_materials(fastgltf::Asset const& asset,
        vkgltf::model_t& model)
    {
        std::set<size_t> unorm_images;
        for (fastgltf::Material const& material : asset.materials)
        {
            vkgltf::material_t m{.name = std::string{material.name},
                .alpha_mode = vkgltf::to_alpha_mode(material.alphaMode),
                .alpha_cutoff = material.alphaCutoff,
                .double_sided = material.doubleSided};

            m.pbr_metallic_roughness.base_color_factor =
                vkgltf::to_glm(material.pbrData.baseColorFactor);
            if (auto const& texture{material.pbrData.baseColorTexture})
            {
                m.pbr_metallic_roughness.base_color_texture =
                    &model.textures[texture->textureIndex];
            }

            if (auto const& texture{material.pbrData.metallicRoughnessTexture})
            {
                m.pbr_metallic_roughness.metallic_roughness_texture =
                    &model.textures[texture->textureIndex];

                unorm_images.insert(m.pbr_metallic_roughness
                        .metallic_roughness_texture->image_index);
            }
            m.pbr_metallic_roughness.metallic_factor =
                material.pbrData.metallicFactor;
            m.pbr_metallic_roughness.roughness_factor =
                material.pbrData.roughnessFactor;

            if (auto const& texture{material.normalTexture})
            {
                m.normal_texture = &model.textures[texture->textureIndex];
                m.normal_scale = texture->scale;

                unorm_images.insert(m.normal_texture->image_index);
            }

            if (auto const& texture{material.emissiveTexture})
            {
                m.emissive_texture = &model.textures[texture->textureIndex];
            }
            m.emissive_factor = vkgltf::to_glm(material.emissiveFactor);
            m.emissive_strength = material.emissiveStrength;

            if (auto const& texture{material.occlusionTexture})
            {
                m.occlusion_texture = &model.textures[texture->textureIndex];
                m.occlusion_strength = texture->strength;
            }

            model.materials.push_back(std::move(m));
        }

        return unorm_images;
    }

    [[nodiscard]] std::expected<loaded_mesh_t, std::error_code>
    load_mesh(fastgltf::Asset const& asset, fastgltf::Mesh const& mesh)
    {
        loaded_mesh_t rv{.name = std::string{mesh.name}};
        rv.primitives.reserve(mesh.primitives.size());

        for (fastgltf::Primitive const& primitive : mesh.primitives)
        {
            bool normals_loaded{false};
            bool tangents_loaded{false};

            loaded_primitive_t p{.topology = vkgltf::to_vulkan(primitive.type)};

            if (auto const* const attr{
                    primitive.findAttribute(position_attribute)};
                attr != primitive.attributes.cend())
            {
                auto const& accessor{asset.accessors[attr->accessorIndex]};
                if (!accessor.bufferViewIndex.has_value())
                {
                    spdlog::error(
                        "Primitive in {} mesh doesn't have position attribute",
                        rv.name);

                    return std::unexpected{make_error_code(
                        vkgltf::error_t::load_transform_failed)};
                }

                p.vertices.resize(accessor.count);
            }
            load_indices(asset, primitive, p.indices);

            auto const vertex_transform =
                [&vertices = p.vertices](glm::vec3 const v, size_t const idx)
            {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
                std::copy_n(glm::value_ptr(v), 3, vertices[idx].position);
            };

            auto const normal_transform =
                [&vertices = p.vertices](glm::vec3 const n, size_t const idx)
            { vertices[idx].normal = n; };

            auto const tangent_transform =
                [&vertices = p.vertices](glm::vec4 const t, size_t const idx)
            { vertices[idx].tangent = t; };

            auto const color_transform = cppext::overloaded{
                [&vertices = p.vertices](glm::vec3 const c, size_t const idx)
                { vertices[idx].color = glm::vec4(c, 1.0f); },
                [&vertices = p.vertices](glm::vec4 const c, size_t const idx)
                { vertices[idx].color = c; }};

            auto const texcoord_transform =
                [&vertices = p.vertices](glm::vec2 const v, size_t const idx)
            { vertices[idx].uv = v; };

            auto const vertex_count{copy_attribute<glm::vec3>(asset,
                primitive,
                position_attribute,
                vertex_transform)};
            if (!vertex_count)
            {
                spdlog::error("Primitive in {} mesh failed to load positions",
                    rv.name);

                return std::unexpected{
                    make_error_code(vkgltf::error_t::load_transform_failed)};
            }
            assert(vertex_count == p.vertices.size());

            if (auto const normal_count{copy_attribute<glm::vec3>(asset,
                    primitive,
                    normal_attribute,
                    normal_transform)})
            {
                assert(normal_count == vertex_count);
                normals_loaded = true;
            }

            if (auto const tangent_count{copy_attribute<glm::vec4>(asset,
                    primitive,
                    tangent_attribute,
                    tangent_transform)})
            {
                assert(tangent_count == vertex_count);
                tangents_loaded = true;
            }

            if (auto const color_count{
                    copy_color_attribute(asset, primitive, color_transform)})
            {
                assert(color_count == vertex_count);
            }

            if (auto const texcoord_count{copy_attribute<glm::vec2>(asset,
                    primitive,
                    texcoord_0_attribute,
                    texcoord_transform)})
            {
                assert(texcoord_count == vertex_count);
            }

            if (!normals_loaded)
            {
                calculate_normals(p);
            }

            if (!tangents_loaded)
            {
                bool const was_indexed{make_unindexed(p)};

                if (!calculate_tangents(p))
                {
                    spdlog::error("Tangent calculation in {} mesh failed",
                        rv.name);

                    return std::unexpected{make_error_code(
                        vkgltf::error_t::load_transform_failed)};
                }

                if (was_indexed)
                {
                    make_indexed(p);
                }
            }

            meshopt_optimizeVertexCache(p.indices.data(),
                p.indices.data(),
                p.indices.size(),
                p.vertices.size());

            meshopt_optimizeOverdraw(p.indices.data(),
                p.indices.data(),
                p.indices.size(),
                &p.vertices[0].position[0],
                p.vertices.size(),
                sizeof(loaded_vertex_t),
                1.05f);

            meshopt_optimizeVertexFetch(p.vertices.data(),
                p.indices.data(),
                p.indices.size(),
                p.vertices.data(),
                p.vertices.size(),
                sizeof(loaded_vertex_t));

            if (primitive.materialIndex)
            {
                p.material_index = *primitive.materialIndex;
            }

            rv.primitives.push_back(std::move(p));
        }

        return rv;
    }

    [[nodiscard]] std::vector<loaded_mesh_t> load_meshes(
        fastgltf::Asset const& asset,
        size_t& vertex_sum,
        size_t& index_sum)
    {
        std::vector<loaded_mesh_t> rv;
        rv.reserve(asset.meshes.size());

        for (fastgltf::Mesh const& mesh : asset.meshes)
        {
            auto load_result{load_mesh(asset, mesh)};
            if (!load_result)
            {
                throw std::system_error{load_result.error()};
            }

            rv.push_back(std::move(*load_result));

            for (auto const& primitive : rv.back().primitives)
            {
                vertex_sum += primitive.vertices.size();
                index_sum += primitive.indices.size();
            }
        }

        return rv;
    }

    void load_node(fastgltf::Asset const& asset,
        size_t const node_index,
        vkgltf::model_t& model,
        std::vector<loaded_mesh_t>& meshes,
        vkgltf::vertex_t*& vertices,
        int32_t& running_vertex_count,
        uint32_t*& indices,
        uint32_t& running_index_count)
    {
        fastgltf::Node const& node{asset.nodes[node_index]};

        vkgltf::node_t n{.name = std::string{node.name}};

        if (node.meshIndex)
        {
            if (auto& mesh{meshes[*node.meshIndex]}; !mesh.consumed)
            {
                auto& model_mesh{model.meshes[*node.meshIndex]};
                model_mesh.name = std::move(mesh.name);
                model_mesh.primitives.reserve(mesh.primitives.size());
                std::ranges::transform(mesh.primitives,
                    std::back_inserter(model_mesh.primitives),
                    [&](loaded_primitive_t& p) mutable
                    {
                        vkgltf::primitive_t rv{
                            .topology = p.topology,
                            .is_indexed = !p.indices.empty(),
                            .material_index = p.material_index,
                        };

                        vertices = std::ranges::transform(p.vertices,
                            vertices,
                            [](loaded_vertex_t const& v) -> vkgltf::vertex_t
                            {
                                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
                                return {.position = glm::make_vec3(v.position),
                                    .normal = v.normal,
                                    .tangent = v.tangent,
                                    .color = v.color,
                                    .uv = v.uv};
                            }).out;

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
                            rv.count =
                                cppext::narrow<uint32_t>(p.indices.size());
                            rv.first =
                                cppext::narrow<uint32_t>(running_index_count);
                            rv.vertex_offset =
                                cppext::narrow<int32_t>(running_vertex_count);
                        }
                        else
                        {
                            rv.count =
                                cppext::narrow<uint32_t>(p.vertices.size());
                            rv.first =
                                cppext::narrow<uint32_t>(running_vertex_count);
                        }

                        running_index_count +=
                            cppext::narrow<uint32_t>(p.indices.size());
                        running_vertex_count +=
                            cppext::narrow<int32_t>(p.vertices.size());

                        return rv;
                    });

                mesh.consumed = true;
            }

            n.mesh = &model.meshes[*node.meshIndex];
        }

        n.matrix = vkgltf::to_glm(fastgltf::getTransformMatrix(node));

        n.child_indices.reserve(node.children.size());
        std::ranges::copy(node.children, std::back_inserter(n.child_indices));

        for (size_t const i : n.child_indices)
        {
            load_node(asset,
                i,
                model,
                meshes,
                vertices,
                running_vertex_count,
                indices,
                running_index_count);
        }

        model.nodes[node_index] = std::move(n);
    }

    void load_scenes(fastgltf::Asset const& asset,
        vkgltf::model_t& model,
        std::vector<loaded_mesh_t>& meshes,
        vkgltf::vertex_t* vertices,
        uint32_t* indices)
    {
        int32_t running_vertex_count{};
        uint32_t running_index_count{};

        for (fastgltf::Scene const& scene : asset.scenes)
        {
            vkgltf::scene_graph_t s{.name = std::string{scene.name}};

            s.root_indices.reserve(scene.nodeIndices.size());
            std::ranges::copy(scene.nodeIndices,
                std::back_inserter(s.root_indices));

            model.scenes.push_back(std::move(s));

            for (size_t const node_index : scene.nodeIndices)
            {
                load_node(asset,
                    node_index,
                    model,
                    meshes,
                    vertices,
                    running_vertex_count,
                    indices,
                    running_index_count);
            }
        }
    }

    DISABLE_WARNING_POP
} // namespace

vkgltf::loader_t::loader_t(vkrndr::backend_t& backend) : backend_{&backend} { }

std::expected<vkgltf::model_t, std::error_code> vkgltf::loader_t::load(
    std::filesystem::path const& path)
{
    std::error_code ec;
    if (auto const file_exists{exists(path, ec)}; !file_exists || ec)
    {
        return std::unexpected{
            ec ? ec : make_error_code(error_t::invalid_file)};
    }

    if (auto const regular_file{is_regular_file(path, ec)}; !regular_file || ec)
    {
        return std::unexpected{
            ec ? ec : make_error_code(error_t::invalid_file)};
    }

    fastgltf::Parser parser{
        fastgltf::Extensions::KHR_materials_emissive_strength};

    auto data{fastgltf::GltfDataBuffer::FromPath(path)};
    if (data.error() != fastgltf::Error::None)
    {
        spdlog::error("Failed to create data buffer: {}", data.error());
        return std::unexpected{make_error_code(translate_error(data.error()))};
    }

    auto const parent_path{path.parent_path()};
    auto asset{parser.loadGltf(data.get(),
        parent_path,
        fastgltf::Options::LoadExternalBuffers)};
    if (asset.error() != fastgltf::Error::None)
    {
        spdlog::error("Failed to load asset: {}", data.error());
        return std::unexpected{make_error_code(translate_error(data.error()))};
    }

    model_t rv;

    try
    {
        load_samplers(asset.get(), rv);
        load_textures(asset.get(), rv);
        std::set<size_t> const unorm_images{load_materials(asset.get(), rv)};
        load_images(backend_, parent_path, unorm_images, asset.get(), rv);

        size_t vertex_count{};
        size_t index_count{};
        std::vector<loaded_mesh_t> meshes{
            load_meshes(asset.get(), vertex_count, index_count)};
        rv.vertex_count = cppext::narrow<uint32_t>(vertex_count);
        rv.vertex_buffer = vkrndr::create_staging_buffer(backend_->device(),
            sizeof(vertex_t) * rv.vertex_count);
        auto vertex_map{
            vkrndr::map_memory(backend_->device(), rv.vertex_buffer)};
        boost::scope::defer_guard const unmap_vertex{[this, &vertex_map]()
            { unmap_memory(backend_->device(), &vertex_map); }};

        auto* const vertices{vertex_map.as<vertex_t>()};

        vkrndr::mapped_memory_t index_map;
        boost::scope::scope_exit unmap_index{[this, &index_map]()
            { unmap_memory(backend_->device(), &index_map); }};

        uint32_t* indices{};
        rv.index_count = cppext::narrow<uint32_t>(index_count);
        if (rv.index_count)
        {
            rv.index_buffer = vkrndr::create_staging_buffer(backend_->device(),
                sizeof(uint32_t) * rv.index_count);
            index_map = vkrndr::map_memory(backend_->device(), rv.index_buffer);
            indices = index_map.as<uint32_t>();
        }
        else
        {
            unmap_index.set_active(false);
        }

        rv.nodes.resize(asset->nodes.size());
        rv.meshes.resize(asset->meshes.size());

        load_scenes(asset.get(), rv, meshes, vertices, indices);
    }
    catch (std::exception const& ex)
    {
        destroy(&backend_->device(), &rv);
        spdlog::error("Failed to load model: {}", ex.what());
        return std::unexpected{make_error_code(error_t::unknown)};
    }

    return rv;
}
