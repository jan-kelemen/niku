#include <optional>
#include <utility>
#include <vkgltf_loader.hpp>

#include <cppext_pragma_warning.hpp>

#include <vkgltf_error.hpp>
#include <vkgltf_fastgltf_adapter.hpp>
#include <vkgltf_model.hpp>

#include <vkrndr_backend.hpp>

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp> // IWYU pragma: keep
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

#include <spdlog/spdlog.h>

#include <tl/expected.hpp>

#include <system_error>

namespace
{
    template<typename T>
    bool copy_attribute(fastgltf::Asset const& asset,
        fastgltf::Primitive const& primitive,
        std::string_view attribute_name,
        std::vector<T>& storage)
    {
        if (auto const* const attr{primitive.findAttribute(attribute_name)};
            attr != primitive.attributes.cend())
        {
            auto const& accessor{asset.accessors[attr->accessorIndex]};
            if (!accessor.bufferViewIndex.has_value())
            {
                return false;
            }

            storage.resize(accessor.count);
            fastgltf::copyFromAccessor<T>(asset, accessor, storage.data());
            return true;
        }

        return false;
    }

    [[nodiscard]] std::optional<vkgltf::index_buffer_t> load_indices(
        fastgltf::Asset const& asset,
        fastgltf::Primitive const& primitive)
    {
        if (!primitive.indicesAccessor.has_value())
        {
            return std::nullopt;
        }

        auto const& accessor{asset.accessors[*primitive.indicesAccessor]};
        if (accessor.componentType == fastgltf::ComponentType::UnsignedByte)
        {
            std::vector<uint8_t> data;
            data.resize(accessor.count);
            fastgltf::copyFromAccessor<uint8_t>(asset, accessor, data.data());
            return vkgltf::index_buffer_t{.buffer = std::move(data)};
        }

        if (accessor.componentType == fastgltf::ComponentType::UnsignedShort)
        {
            std::vector<uint16_t> data;
            data.resize(accessor.count);
            fastgltf::copyFromAccessor<uint16_t>(asset, accessor, data.data());
            return vkgltf::index_buffer_t{.buffer = std::move(data)};
        }

        if (accessor.componentType == fastgltf::ComponentType::UnsignedInt)
        {
            std::vector<uint32_t> data;
            data.resize(accessor.count);
            fastgltf::copyFromAccessor<uint32_t>(asset, accessor, data.data());
            return vkgltf::index_buffer_t{.buffer = std::move(data)};
        }

        spdlog::error("Unrecognized index buffer type: {}",
            std::to_underlying(accessor.componentType));

        return std::nullopt;
    }

    DISABLE_WARNING_PUSH
    DISABLE_WARNING_MISSING_FIELD_INITIALIZERS

    void load_meshes(fastgltf::Asset const& asset, vkgltf::model_t& model)
    {
        static constexpr auto position_attribute{"POSITION"};
        static constexpr auto normal_attribute{"NORMAL"};
        static constexpr auto tangent_attribute{"TANGENT"};
        static constexpr auto uv_attribute{"TEXCOORD_0"};

        model.meshes.reserve(asset.meshes.size());
        for (fastgltf::Mesh const& mesh : asset.meshes)
        {
            vkgltf::mesh_t m{.name{mesh.name}};
            for (fastgltf::Primitive const& primitive : mesh.primitives)
            {
                vkgltf::primitive_t p{
                    .topology = vkgltf::to_vulkan(primitive.type)};

                copy_attribute(asset,
                    primitive,
                    position_attribute,
                    p.positions);
                copy_attribute(asset, primitive, normal_attribute, p.normals);
                copy_attribute(asset, primitive, tangent_attribute, p.tangents);
                copy_attribute(asset, primitive, uv_attribute, p.uvs);

                p.indices = load_indices(asset, primitive);

                m.primitives.push_back(std::move(p));
            }

            model.meshes.push_back(std::move(m));
        }
    }

    DISABLE_WARNING_POP
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
    try
    {
        load_meshes(asset.get(), rv);
    }
    catch (std::exception const& ex)
    {
        destroy(&backend_->device(), &rv);
        spdlog::error("Failed to load model: {}", ex.what());
        return tl::make_unexpected(make_error_code(error_t::unknown));
    }

    return rv;
}
