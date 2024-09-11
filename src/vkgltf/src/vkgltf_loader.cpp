#include <vkgltf_loader.hpp>

#include <cppext_overloaded.hpp>
#include <cppext_pragma_warning.hpp>

#include <vkgltf_error.hpp>
#include <vkgltf_fastgltf_adapter.hpp>
#include <vkgltf_model.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_memory.hpp>

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp> // IWYU pragma: keep
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

#include <glm/gtc/matrix_transform.hpp>

#include <spdlog/spdlog.h>

#include <tl/expected.hpp>

#include <optional>
#include <system_error>
#include <utility>
#include <variant>

namespace
{
    constexpr auto position_attribute{"POSITION"};

    template<typename T>
    [[nodiscard]] uint32_t copy_attribute(fastgltf::Asset const& asset,
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
                return 0;
            }

            fastgltf::iterateAccessorWithIndex<T>(asset, accessor, transform);

            return accessor.count;
        }

        return 0;
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

    DISABLE_WARNING_PUSH
    DISABLE_WARNING_MISSING_FIELD_INITIALIZERS

    void load_meshes(fastgltf::Asset const& asset,
        vkgltf::model_t& model,
        vkgltf::vertex_t* vertices,
        uint32_t* indices)
    {
        uint32_t running_index_count{};
        int32_t running_vertex_count{};

        auto vertex_transform = [&vertices](glm::vec3 const v, size_t const idx)
        { vertices[idx] = {.position = v}; };

        model.meshes.reserve(asset.meshes.size());
        for (fastgltf::Mesh const& mesh : asset.meshes)
        {
            vkgltf::mesh_t m{.name = std::string{mesh.name}};
            for (fastgltf::Primitive const& primitive : mesh.primitives)
            {
                vkgltf::primitive_t p{
                    .topology = vkgltf::to_vulkan(primitive.type)};

                size_t const vertex_count = copy_attribute<glm::vec3>(asset,
                    primitive,
                    position_attribute,
                    vertex_transform);
                vertices += vertex_count;

                size_t const index_count =
                    load_indices(asset, primitive, indices);
                indices += index_count;

                p.is_indexed = index_count != 0;
                if (p.is_indexed)
                {
                    p.count = index_count;
                    p.first = running_index_count;
                    p.vertex_offset = running_vertex_count;
                }
                else
                {
                    p.count = vertex_count;
                    p.first = running_vertex_count;
                }

                running_vertex_count += vertex_count;
                running_index_count += index_count;

                m.primitives.push_back(std::move(p));
            }

            model.meshes.push_back(std::move(m));
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
                    model.vertex_count += accessor.count;
                }

                if (primitive.indicesAccessor.has_value())
                {
                    auto const& accessor{
                        asset.accessors[*primitive.indicesAccessor]};
                    model.index_count += accessor.count;
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

    auto asset{parser.loadGltf(data.get(),
        path.parent_path(),
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
        load_meshes(asset.get(), rv, vertices, indices);
        unmap_memory(backend_->device(), &vertex_map);
        if (indices)
        {
            unmap_memory(backend_->device(), &index_map);
        }
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
