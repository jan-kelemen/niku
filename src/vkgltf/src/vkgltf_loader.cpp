#include <vkgltf_loader.hpp>

#include <cppext_numeric.hpp>
#include <cppext_overloaded.hpp>
#include <cppext_pragma_warning.hpp>

#include <vkgltf_error.hpp>
#include <vkgltf_fastgltf_adapter.hpp>
#include <vkgltf_model.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_buffer.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_memory.hpp>
#include <vkrndr_utility.hpp>

#include <stb_image.h>

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp> // IWYU pragma: keep
#include <fastgltf/math.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <spdlog/spdlog.h>

#include <tl/expected.hpp>

#include <volk.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

// IWYU pragma: no_include <fmt/base.h>
// IWYU pragma: no_include <glm/detail/qualifier.hpp>
// IWYU pragma: no_include <functional>

namespace
{
    constexpr auto position_attribute{"POSITION"};
    constexpr auto texcoord_0_attribute{"TEXCOORD_0"};

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

    [[nodiscard]] size_t load_indices(fastgltf::Asset const& asset,
        fastgltf::Primitive const& primitive,
        uint32_t* indices)
    {
        if (!primitive.indicesAccessor.has_value())
        {
            return 0;
        }

        auto const indices_transform =
            [&indices](auto const v, size_t const idx) { indices[idx] = v; };

        auto const& accessor{asset.accessors[*primitive.indicesAccessor]};
        if (accessor.componentType == fastgltf::ComponentType::UnsignedByte)
        {
            fastgltf::iterateAccessorWithIndex<uint8_t>(asset,
                accessor,
                indices_transform);
            return accessor.count;
        }

        if (accessor.componentType == fastgltf::ComponentType::UnsignedShort)
        {
            fastgltf::iterateAccessorWithIndex<uint16_t>(asset,
                accessor,
                indices_transform);
            return accessor.count;
        }

        if (accessor.componentType == fastgltf::ComponentType::UnsignedInt)
        {
            fastgltf::copyFromAccessor<uint32_t>(asset, accessor, indices);
            return accessor.count;
        }

        spdlog::error("Unrecognized index buffer type: {}",
            std::to_underlying(accessor.componentType));

        return 0;
    }

    [[nodiscard]] tl::expected<vkrndr::image_t, std::error_code> load_image(
        vkrndr::backend_t* const backend,
        std::filesystem::path const& parent_path,
        fastgltf::Asset const& asset,
        fastgltf::Image const& image)
    {
        auto const transfer_image = [backend](int const width,
                                        int const height,
                                        stbi_uc const* const data)
            -> tl::expected<vkrndr::image_t, std::error_code>
        {
            if (data)
            {
                VkExtent2D const extent{cppext::narrow<uint32_t>(width),
                    cppext::narrow<uint32_t>(height)};
                // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)
                std::span<std::byte const> const image_data{
                    reinterpret_cast<std::byte const*>(data),
                    size_t{extent.width} * extent.height * 4};
                // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)

                return backend->transfer_image(image_data,
                    extent,
                    VK_FORMAT_R8G8B8A8_SRGB,
                    vkrndr::max_mip_levels(width, height));
            }

            return tl::make_unexpected(
                make_error_code(vkgltf::error_t::invalid_file));
        };

        auto const load_from_vector =
            [&](fastgltf::sources::Vector const& vector)
        {
            int width; // NOLINT
            int height; // NOLINT
            int channels; // NOLINT
            // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)
            stbi_uc* const data{stbi_load_from_memory(
                reinterpret_cast<stbi_uc const*>(vector.bytes.data()),
                cppext::narrow<int>(vector.bytes.size()),
                &width,
                &height,
                &channels,
                4)};
            // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
            return transfer_image(width, height, data);
        };

        auto const unsupported_variant = []([[maybe_unused]] auto&& arg)
            -> tl::expected<vkrndr::image_t, std::error_code>
        {
            spdlog::error("Unsupported variant during image load");
            return tl::make_unexpected(
                make_error_code(vkgltf::error_t::unknown));
        };

        return std::visit(
            cppext::overloaded{
                [&transfer_image, &parent_path](
                    fastgltf::sources::URI const& filePath)
                    -> tl::expected<vkrndr::image_t, std::error_code>
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
                        return tl::make_unexpected(
                            make_error_code(vkgltf::error_t::unknown));
                    }

                    auto rv{transfer_image(width, height, data)};

                    stbi_image_free(data);

                    return rv;
                },
                load_from_vector,
                [&unsupported_variant, &load_from_vector, &asset](
                    fastgltf::sources::BufferView const& view)
                {
                    auto const& bufferView =
                        asset.bufferViews[view.bufferViewIndex];
                    auto const& buffer = asset.buffers[bufferView.bufferIndex];

                    return std::visit(cppext::overloaded{unsupported_variant,
                                          load_from_vector},
                        buffer.data);
                },
                unsupported_variant},
            image.data);
    }

    void load_images(vkrndr::backend_t* const backend,
        std::filesystem::path const& parent_path,
        fastgltf::Asset const& asset,
        vkgltf::model_t& model)
    {
        for (fastgltf::Image const& image : asset.images)
        {
            auto load_result{load_image(backend, parent_path, asset, image)};
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

    void load_materials(fastgltf::Asset const& asset, vkgltf::model_t& model)
    {
        for (fastgltf::Material const& material : asset.materials)
        {
            vkgltf::material_t m{.name = std::string{material.name},
                .double_sided = material.doubleSided};
            m.pbr_metallic_roughness.base_color_factor =
                vkgltf::to_glm(material.pbrData.baseColorFactor);
            if (material.pbrData.baseColorTexture)
            {
                m.pbr_metallic_roughness.base_color_texture =
                    &model.textures[material.pbrData.baseColorTexture
                                        ->textureIndex];
            }

            model.materials.push_back(std::move(m));
        }
    }

    void load_meshes(fastgltf::Asset const& asset,
        vkgltf::model_t& model,
        vkgltf::vertex_t* vertices,
        uint32_t* indices)
    {
        uint32_t running_index_count{};
        int32_t running_vertex_count{};

        auto const vertex_transform =
            [&vertices](glm::vec3 const v, size_t const idx)
        { vertices[idx] = {.position = v}; };

        auto const texcoord_transform =
            [&vertices](glm::vec2 const v, size_t const idx)
        { vertices[idx].uv = v; };

        model.meshes.reserve(asset.meshes.size());
        for (fastgltf::Mesh const& mesh : asset.meshes)
        {
            vkgltf::mesh_t m{.name = std::string{mesh.name}};
            for (fastgltf::Primitive const& primitive : mesh.primitives)
            {
                vkgltf::primitive_t p{
                    .topology = vkgltf::to_vulkan(primitive.type)};

                auto const vertex_count{copy_attribute<glm::vec3>(asset,
                    primitive,
                    position_attribute,
                    vertex_transform)};
                assert(vertex_count);

                if (auto const texcoord_count{copy_attribute<glm::vec2>(asset,
                        primitive,
                        texcoord_0_attribute,
                        texcoord_transform)})
                {
                    assert(texcoord_count == vertex_count);
                }

                vertices += *vertex_count;

                size_t const index_count{
                    load_indices(asset, primitive, indices)};
                indices += index_count;

                p.is_indexed = index_count != 0;
                if (p.is_indexed)
                {
                    p.count = cppext::narrow<uint32_t>(index_count);
                    p.first = cppext::narrow<uint32_t>(running_index_count);
                    p.vertex_offset =
                        cppext::narrow<int32_t>(running_vertex_count);
                }
                else
                {
                    p.count = cppext::narrow<uint32_t>(*vertex_count);
                    p.first = cppext::narrow<uint32_t>(running_vertex_count);
                }

                running_vertex_count += cppext::narrow<int32_t>(*vertex_count);
                running_index_count += cppext::narrow<uint32_t>(index_count);

                if (primitive.materialIndex)
                {
                    p.material_index = *primitive.materialIndex;
                }

                m.primitives.push_back(p);
            }

            model.meshes.push_back(std::move(m));
        }
    }

    void load_nodes(fastgltf::Asset const& asset, vkgltf::model_t& model)
    {
        for (fastgltf::Node const& node : asset.nodes)
        {
            vkgltf::node_t n{.name = std::string{node.name}};

            if (node.meshIndex)
            {
                n.mesh = &model.meshes[*node.meshIndex];
            }

            n.matrix = vkgltf::to_glm(fastgltf::getTransformMatrix(node));

            n.child_indices.reserve(node.children.size());
            std::ranges::copy(node.children,
                std::back_inserter(n.child_indices));

            model.nodes.push_back(std::move(n));
        }
    }

    void load_scenes(fastgltf::Asset const& asset, vkgltf::model_t& model)
    {
        for (fastgltf::Scene const& scene : asset.scenes)
        {
            vkgltf::scene_graph_t s{.name = std::string{scene.name}};

            s.root_indices.reserve(scene.nodeIndices.size());
            std::ranges::copy(scene.nodeIndices,
                std::back_inserter(s.root_indices));

            model.scenes.push_back(std::move(s));
        }
    }

    DISABLE_WARNING_POP

    void collect_primitive_data(fastgltf::Asset const& asset,
        vkgltf::model_t& model)
    {
        for (fastgltf::Mesh const& mesh : asset.meshes)
        {
            for (fastgltf::Primitive const& primitive : mesh.primitives)
            {
                auto const* const attribute{
                    primitive.findAttribute(position_attribute)};
                if (attribute != primitive.attributes.cend())
                {
                    auto const& accessor{
                        asset.accessors[attribute->accessorIndex]};
                    model.vertex_count +=
                        cppext::narrow<uint32_t>(accessor.count);
                }

                if (primitive.indicesAccessor.has_value())
                {
                    auto const& accessor{
                        asset.accessors[*primitive.indicesAccessor]};
                    model.index_count +=
                        cppext::narrow<uint32_t>(accessor.count);
                }
            }
        }
    }
} // namespace

vkgltf::loader_t::loader_t(vkrndr::backend_t* const backend) : backend_{backend}
{
}

tl::expected<vkgltf::model_t, std::error_code> vkgltf::loader_t::load(
    std::filesystem::path const& path)
{
    std::error_code ec;
    if (auto const file_exists{exists(path, ec)}; !file_exists || ec)
    {
        return tl::make_unexpected(
            ec ? ec : make_error_code(error_t::invalid_file));
    }

    if (auto const regular_file{is_regular_file(path, ec)}; !regular_file || ec)
    {
        return tl::make_unexpected(
            ec ? ec : make_error_code(error_t::invalid_file));
    }

    fastgltf::Parser parser;

    auto data{fastgltf::GltfDataBuffer::FromPath(path)};
    if (data.error() != fastgltf::Error::None)
    {
        spdlog::error("Failed to create data buffer: {}", data.error());
        return tl::make_unexpected(
            make_error_code(translate_error(data.error())));
    }

    auto const parent_path{path.parent_path()};
    auto asset{parser.loadGltf(data.get(),
        parent_path,
        fastgltf::Options::LoadExternalBuffers)};
    if (asset.error() != fastgltf::Error::None)
    {
        spdlog::error("Failed to load asset: {}", data.error());
        return tl::make_unexpected(
            make_error_code(translate_error(data.error())));
    }

    model_t rv;
    collect_primitive_data(asset.get(), rv);
    rv.vertex_buffer = vkrndr::create_buffer(backend_->device(),
        sizeof(vertex_t) * rv.vertex_count,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    auto vertex_map{vkrndr::map_memory(backend_->device(), rv.vertex_buffer)};
    auto* const vertices{vertex_map.as<vertex_t>()};

    vkrndr::mapped_memory_t index_map;
    uint32_t* indices{};
    if (rv.index_count != 0)
    {
        rv.index_buffer = vkrndr::create_buffer(backend_->device(),
            sizeof(uint32_t) * rv.index_count,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        index_map = vkrndr::map_memory(backend_->device(), rv.index_buffer);
        indices = index_map.as<uint32_t>();
    }

    try
    {
        load_images(backend_, parent_path, asset.get(), rv);
        load_samplers(asset.get(), rv);
        load_textures(asset.get(), rv);
        load_materials(asset.get(), rv);

        load_meshes(asset.get(), rv, vertices, indices);
        unmap_memory(backend_->device(), &vertex_map);
        if (indices)
        {
            unmap_memory(backend_->device(), &index_map);
        }

        load_nodes(asset.get(), rv);
        load_scenes(asset.get(), rv);
    }
    catch (std::exception const& ex)
    {
        unmap_memory(backend_->device(), &vertex_map);
        if (indices)
        {
            unmap_memory(backend_->device(), &index_map);
        }
        destroy(&backend_->device(), &rv);
        spdlog::error("Failed to load model: {}", ex.what());
        return tl::make_unexpected(make_error_code(error_t::unknown));
    }

    return rv;
}
