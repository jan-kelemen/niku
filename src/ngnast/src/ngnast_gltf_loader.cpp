#include <ngnast_gltf_loader.hpp>

#include <ngnast_error.hpp>
#include <ngnast_gltf_fastgltf_adapter.hpp>
#include <ngnast_mesh_transform.hpp>
#include <ngnast_scene_model.hpp>

#include <cppext_numeric.hpp>
#include <cppext_overloaded.hpp>

#include <vkrndr_utility.hpp>

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp> // IWYU pragma: keep
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <libbasisu/transcoder/basisu_transcoder.h>

#include <meshoptimizer.h>

#include <mikktspace.h>

#include <spdlog/spdlog.h>

#include <stb_image.h>

#include <volk.h>

#include <vulkan/utility/vk_format_utils.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <exception> // IWYU pragma: keep
#include <expected>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <ranges>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
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
    [[nodiscard]] std::vector<char> read_file(std::filesystem::path const& file)
    {
        std::ifstream stream{file, std::ios::ate | std::ios::binary};

        if (!stream.is_open())
        {
            throw std::runtime_error{"failed to open file!"};
        }

        auto const eof{stream.tellg()};

        std::vector<char> buffer(static_cast<size_t>(eof));
        stream.seekg(0);

        stream.read(buffer.data(), eof);

        return buffer;
    }

    [[nodiscard]] std::expected<ngnast::image_t, std::error_code>
    load_ktx_image(VkFormat const format,
        std::span<char const> const& raw_bytes)
    {
        basist::ktx2_transcoder transcoder;
        bool const transcoder_initialized{transcoder.init(raw_bytes.data(),
            cppext::narrow<uint32_t>(raw_bytes.size()))};
        if (!transcoder_initialized)
        {
            return std::unexpected{
                make_error_code(ngnast::error_t::invalid_file)};
        }
        bool const transcoding_started{transcoder.start_transcoding()};
        if (!transcoding_started)
        {
            return std::unexpected{
                make_error_code(ngnast::error_t::invalid_file)};
        }

        ngnast::image_t image{
            .data = std::unique_ptr<std::byte[], void (*)(std::byte*)>{nullptr,
                [](std::byte* const p)
                {
                    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
                    delete[] p;
                }},
            .format = format,
        };

        auto const transcoder_texture_format{std::invoke(
            [](VkFormat const f)
            {
                using basist::transcoder_texture_format;
                switch (f)
                {
                case VK_FORMAT_BC7_SRGB_BLOCK:
                case VK_FORMAT_BC7_UNORM_BLOCK:
                    return transcoder_texture_format::cTFBC7_RGBA;
                case VK_FORMAT_BC3_SRGB_BLOCK:
                case VK_FORMAT_BC3_UNORM_BLOCK:
                    return transcoder_texture_format::cTFBC3_RGBA;
                case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
                case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
                    return transcoder_texture_format::cTFASTC_4x4_RGBA;
                case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
                case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
                    return transcoder_texture_format::cTFETC2_RGBA;
                default:
                    assert(f == VK_FORMAT_R8G8B8A8_SRGB ||
                        f == VK_FORMAT_R8G8B8A8_UNORM);
                    return transcoder_texture_format::cTFRGBA32;
                }
            },
            image.format)};

        bool const load_compressed{transcoder_texture_format !=
            basist::transcoder_texture_format::cTFRGBA32};

        uint32_t const bytes_per_block{
            basis_get_bytes_per_block_or_pixel(transcoder_texture_format)};

        std::vector<uint32_t> bytes_per_mip;
        for (uint32_t mip{}; mip != transcoder.get_levels(); ++mip)
        {
            basist::ktx2_image_level_info mip_info{};
            bool const mip_info_loaded{
                transcoder.get_image_level_info(mip_info, mip, 0, 0)};
            assert(mip_info_loaded);
            if (!mip_info_loaded)
            {
                break;
            }

            uint32_t const pixel_or_block_count{load_compressed
                    ? mip_info.m_total_blocks
                    : mip_info.m_orig_width * mip_info.m_orig_height};

            if (uint32_t mip_size{};
                cppext::mul(bytes_per_block, pixel_or_block_count, mip_size))
            {
                bytes_per_mip.push_back(mip_size);
            }
            else
            {
                return std::unexpected{
                    make_error_code(ngnast::error_t::invalid_file)};
            }
        }

        size_t running_mip_offset{};
        for (uint32_t mip{}; mip != transcoder.get_levels(); ++mip)
        {
            basist::ktx2_image_level_info mip_info{};
            if (!transcoder.get_image_level_info(mip_info, mip, 0, 0))
            {
                return std::unexpected{
                    make_error_code(ngnast::error_t::load_transform_failed)};
            }

            ngnast::image_mip_level_t const image_mip_level{
                .extent = vkrndr::to_2d_extent(mip_info.m_orig_width,
                    mip_info.m_orig_height),
                .data_offset = running_mip_offset,
                .data_size = bytes_per_mip[mip],
            };

            if (mip == 0)
            {
                image.data_size = std::reduce(std::cbegin(bytes_per_mip),
                    std::cend(bytes_per_mip),
                    size_t{},
                    [](size_t const acc, uint32_t const size)
                    { return acc + size; });

                // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
                image.data.reset(new std::byte[image.data_size]);
            }

            bool const image_level_transcoded{
                transcoder.transcode_image_level(mip,
                    0,
                    0,
                    image.data.get() + image_mip_level.data_offset,
                    cppext::narrow<uint32_t>(image_mip_level.data_size),
                    transcoder_texture_format)};
            if (!image_level_transcoded)
            {
                return std::unexpected{
                    make_error_code(ngnast::error_t::invalid_file)};
            }

            image.mip_levels.push_back(image_mip_level);
            running_mip_offset += image_mip_level.data_size;
        }

        return image;
    }

    constexpr auto position_attribute{"POSITION"};
    constexpr auto normal_attribute{"NORMAL"};
    constexpr auto tangent_attribute{"TANGENT"};
    constexpr auto texcoord_0_attribute{"TEXCOORD_0"};
    constexpr auto color_attribute{"COLOR_0"};

    void calculate_normals(ngnast::primitive_t& primitive)
    {
        bool const is_indexed{!primitive.indices.empty()};

        auto& vertices{primitive.vertices};
        auto const& indices{primitive.indices};

        size_t const vertex_count{
            is_indexed ? primitive.indices.size() : primitive.vertices.size()};
        for (size_t i{}; i != vertex_count; i += 3)
        {
            auto& point1{is_indexed ? vertices[indices[i]] : vertices[i]};
            auto& point2{
                is_indexed ? vertices[indices[i + 1]] : vertices[i + 1]};
            auto& point3{
                is_indexed ? vertices[indices[i + 2]] : vertices[i + 2]};

            glm::vec3 const edge1{point2.position - point1.position};
            glm::vec3 const edge2{point3.position - point1.position};

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

    [[nodiscard]] bool calculate_tangents(ngnast::primitive_t& primitive)
    {
        auto const get_num_faces = [](SMikkTSpaceContext const* context) -> int
        {
            auto* const v{static_cast<std::vector<ngnast::vertex_t>*>(
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
            auto const& v{*static_cast<std::vector<ngnast::vertex_t>*>(
                context->m_pUserData)};

            std::copy_n(
                glm::value_ptr(
                    v[cppext::narrow<size_t>(face * 3 + vertex)].position),
                3,
                out);
        };

        auto const get_normal = [](SMikkTSpaceContext const* context,
                                    float* const out,
                                    int const face,
                                    int const vertex)
        {
            auto const& v{*static_cast<std::vector<ngnast::vertex_t>*>(
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
            auto const& v{*static_cast<std::vector<ngnast::vertex_t>*>(
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
            auto& v{*static_cast<std::vector<ngnast::vertex_t>*>(
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

    [[nodiscard]] std::expected<ngnast::image_t, std::error_code> load_image(
        std::span<VkFormat> const& compressed_formats,
        std::filesystem::path const& parent_path,
        bool const as_unorm,
        fastgltf::Asset const& asset,
        fastgltf::Image const& image)
    {
        auto const select_compressed_texture_format =
            [&compressed_formats, as_unorm]() -> std::optional<VkFormat>
        {
            if (auto it{std::ranges::find_if(compressed_formats,
                    as_unorm ? vkuFormatIsUNORM : vkuFormatIsSRGB)};
                it != std::cend(compressed_formats))
            {
                return *it;
            }
            return std::nullopt;
        };

        auto const image_from_data =
            [as_unorm](int const width, int const height, stbi_uc* const data)
            -> std::expected<ngnast::image_t, std::error_code>
        {
            if (data)
            {
                // Temporary variable to ensure data is released in case of
                // exception
                // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)
                std::unique_ptr<std::byte[], void (*)(std::byte*)> image_data{
                    reinterpret_cast<std::byte*>(data),
                    [](std::byte* p)
                    { stbi_image_free(reinterpret_cast<stbi_uc*>(p)); }};
                // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)

                size_t const size{cppext::narrow<size_t>(width) *
                    cppext::narrow<size_t>(height) *
                    4};

                ngnast::image_t rv{.data = std::move(image_data),
                    .data_size = size,
                    .format = as_unorm ? VK_FORMAT_R8G8B8A8_UNORM
                                       : VK_FORMAT_R8G8B8A8_SRGB,
                    .mip_levels = std::vector{ngnast::image_mip_level_t{
                        vkrndr::to_2d_extent(width, height),
                        0,
                        size}}};

                return rv;
            }

            return std::unexpected{
                make_error_code(ngnast::error_t::invalid_file)};
        };

        auto const load_from_container = [&select_compressed_texture_format,
                                             &image_from_data]<typename T>(
                                             fastgltf::MimeType const mime_type,
                                             T const& container,
                                             size_t const offset,
                                             size_t const size)
            -> std::expected<ngnast::image_t, std::error_code>
        {
            if (mime_type == fastgltf::MimeType::KTX2)
            {
                std::optional<VkFormat> const format{
                    select_compressed_texture_format()};
                if (!format)
                {
                    return std::unexpected{make_error_code(
                        ngnast::error_t::load_transform_failed)};
                }

                return load_ktx_image(*format,
                    std::span{
                        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                        reinterpret_cast<char const*>(container.bytes.data()) +
                            offset,
                        size});
            }

            int width; // NOLINT
            int height; // NOLINT
            int channels; // NOLINT
            auto* const data{stbi_load_from_memory(
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                reinterpret_cast<stbi_uc const*>(container.bytes.data()) +
                    offset,
                cppext::narrow<int>(size),
                &width,
                &height,
                &channels,
                4)};

            return image_from_data(width, height, data);
        };

        auto const load_from_vector =
            [&load_from_container](fastgltf::MimeType const mime_type,
                fastgltf::sources::Vector const& container,
                size_t const offset,
                size_t const size)
        { return load_from_container(mime_type, container, offset, size); };

        auto const load_from_array =
            [&load_from_container](fastgltf::MimeType const mime_type,
                fastgltf::sources::Array const& container,
                size_t const offset,
                size_t const size)
        { return load_from_container(mime_type, container, offset, size); };

        auto const load_from_uri =
            [&select_compressed_texture_format, &image_from_data, &parent_path](
                fastgltf::sources::URI const& filePath)
            -> std::expected<ngnast::image_t, std::error_code>
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

            fastgltf::MimeType effective_mime_type{filePath.mimeType};
            if (filePath.mimeType == fastgltf::MimeType::None)
            {
                if (path.extension() == ".ktx2")
                {
                    effective_mime_type = fastgltf::MimeType::KTX2;
                }
            }

            switch (effective_mime_type)
            {
            case fastgltf::MimeType::KTX2:
            {
                std::optional<VkFormat> const format{
                    select_compressed_texture_format()};
                if (!format)
                {
                    return std::unexpected{make_error_code(
                        ngnast::error_t::load_transform_failed)};
                }

                std::vector<char> raw_bytes{read_file(path)};
                return load_ktx_image(*format, raw_bytes);
            }
            default:
            {
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
                        make_error_code(ngnast::error_t::unknown)};
                }
                return image_from_data(width, height, data);
            }
            }
        };

        auto const unsupported_variant = []([[maybe_unused]] auto&& arg)
            -> std::expected<ngnast::image_t, std::error_code>
        {
            spdlog::error("Unsupported variant '{}' during image load",
                typeid(arg).name());
            return std::unexpected{make_error_code(ngnast::error_t::unknown)};
        };

        auto const load_from_buffer_view =
            [&unsupported_variant, load_from_array, load_from_vector, &asset](
                fastgltf::sources::BufferView const& view)
        {
            auto const& bufferView = asset.bufferViews[view.bufferViewIndex];
            auto const& buffer = asset.buffers[bufferView.bufferIndex];
            return std::visit(
                cppext::overloaded{
                    [mt = view.mimeType,
                        off = bufferView.byteOffset,
                        size = bufferView.byteLength,
                        &load_from_array](fastgltf::sources::Array const& array)
                    { return load_from_array(mt, array, off, size); },
                    [mt = view.mimeType,
                        off = bufferView.byteOffset,
                        size = bufferView.byteLength,
                        &load_from_vector](
                        fastgltf::sources::Vector const& vector)
                    { return load_from_vector(mt, vector, off, size); },
                    unsupported_variant},
                buffer.data);
        };

        return std::visit(
            cppext::overloaded{load_from_uri,
                [&load_from_array](fastgltf::sources::Array const& array)
                {
                    return load_from_array(array.mimeType,
                        array,
                        0,
                        array.bytes.size_bytes());
                },
                [&load_from_vector](fastgltf::sources::Vector const& vector)
                {
                    return load_from_vector(vector.mimeType,
                        vector,
                        0,
                        vector.bytes.size());
                },
                load_from_buffer_view,
                unsupported_variant},
            image.data);
    }

    void load_images(std::span<VkFormat> const& compressed_texture_formats,
        std::filesystem::path const& parent_path,
        std::set<size_t> const& unorm_images,
        fastgltf::Asset const& asset,
        ngnast::scene_model_t& model)
    {
        for (size_t i{}; i != asset.images.size(); ++i)
        {
            auto const& image{asset.images[i]};

            auto load_result{load_image(compressed_texture_formats,
                parent_path,
                unorm_images.contains(i),
                asset,
                image)};
            if (!load_result)
            {
                throw std::system_error{load_result.error()};
            }

            model.images.push_back(std::move(load_result).value());
        }
    }

    void load_samplers(fastgltf::Asset const& asset,
        ngnast::scene_model_t& model)
    {
        model.samplers.reserve(asset.samplers.size() + 1);
        for (fastgltf::Sampler const& sampler : asset.samplers)
        {
            model.samplers.emplace_back(
                ngnast::gltf::to_vulkan(
                    sampler.magFilter.value_or(fastgltf::Filter::Nearest)),
                ngnast::gltf::to_vulkan(
                    sampler.minFilter.value_or(fastgltf::Filter::Nearest)),
                ngnast::gltf::to_vulkan(sampler.wrapS),
                ngnast::gltf::to_vulkan(sampler.wrapT),
                ngnast::gltf::to_vulkan_mipmap(
                    sampler.minFilter.value_or(fastgltf::Filter::Nearest)));
        }

        model.samplers.emplace_back(); // Add default sampler to the end
    }

    void load_textures(fastgltf::Asset const& asset,
        ngnast::scene_model_t& model)
    {
        for (fastgltf::Texture const& texture : asset.textures)
        {
            ngnast::texture_t t{.name = std::string{texture.name}};

            if (texture.imageIndex)
            {
                t.image_indices.emplace(ngnast::texture_image_type_t::regular,
                    *texture.imageIndex);
            }

            if (texture.basisuImageIndex)
            {
                t.image_indices.emplace(ngnast::texture_image_type_t::basisu,
                    *texture.basisuImageIndex);
            }

            t.sampler_index =
                texture.samplerIndex.value_or(model.samplers.size() - 1);

            model.textures.push_back(std::move(t));
        }
    }

    [[nodiscard]] std::set<size_t> load_materials(fastgltf::Asset const& asset,
        ngnast::scene_model_t& model)
    {
        std::set<size_t> unorm_images;
        for (fastgltf::Material const& material : asset.materials)
        {
            ngnast::material_t m{.name = std::string{material.name},
                .alpha_mode = ngnast::gltf::to_alpha_mode(material.alphaMode),
                .alpha_cutoff = material.alphaCutoff,
                .double_sided = material.doubleSided};

            m.pbr_metallic_roughness.base_color_factor =
                ngnast::gltf::to_glm(material.pbrData.baseColorFactor);
            if (auto const& texture{material.pbrData.baseColorTexture})
            {
                m.pbr_metallic_roughness.base_color_texture =
                    &model.textures[texture->textureIndex];
            }

            if (auto const& texture{material.pbrData.metallicRoughnessTexture})
            {
                auto& material_texture{
                    m.pbr_metallic_roughness.metallic_roughness_texture};
                material_texture = &model.textures[texture->textureIndex];

                for (auto const& [type, index] :
                    material_texture->image_indices)
                {
                    unorm_images.emplace(index);
                }
            }
            m.pbr_metallic_roughness.metallic_factor =
                material.pbrData.metallicFactor;
            m.pbr_metallic_roughness.roughness_factor =
                material.pbrData.roughnessFactor;

            if (auto const& texture{material.normalTexture})
            {
                m.normal_texture = &model.textures[texture->textureIndex];
                m.normal_scale = texture->scale;

                for (auto const& [type, index] :
                    m.normal_texture->image_indices)
                {
                    unorm_images.emplace(index);
                }
            }

            if (auto const& texture{material.emissiveTexture})
            {
                m.emissive_texture = &model.textures[texture->textureIndex];
            }
            m.emissive_factor = ngnast::gltf::to_glm(material.emissiveFactor);
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

    [[nodiscard]] std::expected<ngnast::mesh_t, std::error_code> load_mesh(
        fastgltf::Asset const& asset,
        fastgltf::Mesh const& mesh,
        std::vector<ngnast::primitive_t>& primitives)
    {
        ngnast::mesh_t rv{.name = std::string{mesh.name}};
        rv.primitive_indices.reserve(mesh.primitives.size());

        assert(!mesh.primitives.empty());

        for (fastgltf::Primitive const& primitive : mesh.primitives)
        {
            bool normals_loaded{false};
            bool tangents_loaded{false};

            ngnast::primitive_t p{
                .topology = ngnast::gltf::to_vulkan(primitive.type)};

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
                        ngnast::error_t::load_transform_failed)};
                }

                p.vertices.resize(accessor.count);

                {
                    if (!accessor.min || !accessor.max)
                    {
                        spdlog::error(
                            "Primitive in {} mesh doesn't have position attribute min/max accessors",
                            rv.name);

                        return std::unexpected{make_error_code(
                            ngnast::error_t::load_transform_failed)};
                    }

                    auto const to_glm = [](auto const& v) -> glm::vec3
                    {
                        return {
                            v->template get<double>(0),
                            v->template get<double>(1),
                            v->template get<double>(2),
                        };
                    };

                    p.bounding_box = {to_glm(accessor.min),
                        to_glm(accessor.max)};
                }
            }
            load_indices(asset, primitive, p.indices);

            auto const vertex_transform =
                [&vertices = p.vertices](glm::vec3 const v, size_t const idx)
            { vertices[idx].position = v; };

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
                    make_error_code(ngnast::error_t::load_transform_failed)};
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
                bool const was_indexed{ngnast::mesh::make_unindexed(p)};

                if (!calculate_tangents(p))
                {
                    spdlog::error("Tangent calculation in {} mesh failed",
                        rv.name);

                    return std::unexpected{make_error_code(
                        ngnast::error_t::load_transform_failed)};
                }

                if (was_indexed)
                {
                    ngnast::mesh::make_indexed(p);
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
                sizeof(ngnast::vertex_t),
                1.05f);

            meshopt_optimizeVertexFetch(p.vertices.data(),
                p.indices.data(),
                p.indices.size(),
                p.vertices.data(),
                p.vertices.size(),
                sizeof(ngnast::vertex_t));

            if (primitive.materialIndex)
            {
                p.material_index = *primitive.materialIndex;
            }

            rv.primitive_indices.push_back(primitives.size());
            primitives.push_back(std::move(p));
        }

        rv.bounding_box = primitives[rv.primitive_indices.front()].bounding_box;
        for (auto const& index : rv.primitive_indices | std::views::drop(1))
        {
            auto const& primitive{primitives[index]};

            rv.bounding_box.min =
                glm::min(rv.bounding_box.min, primitive.bounding_box.min);
            rv.bounding_box.max =
                glm::max(rv.bounding_box.max, primitive.bounding_box.max);
        }

        return rv;
    }

    [[nodiscard]] std::vector<ngnast::mesh_t> load_meshes(
        fastgltf::Asset const& asset,
        std::vector<ngnast::primitive_t>& primitives)
    {
        std::vector<ngnast::mesh_t> rv;
        rv.reserve(asset.meshes.size());

        for (fastgltf::Mesh const& mesh : asset.meshes)
        {
            auto load_result{load_mesh(asset, mesh, primitives)};
            if (!load_result)
            {
                throw std::system_error{load_result.error()};
            }

            rv.push_back(std::move(*load_result));
        }

        return rv;
    }

    void load_node(fastgltf::Asset const& asset,
        size_t const node_index,
        ngnast::scene_model_t& model)
    {
        fastgltf::Node const& node{asset.nodes[node_index]};

        ngnast::node_t n{.name = std::string{node.name}};

        if (node.meshIndex)
        {
            n.mesh_index = *node.meshIndex;

            auto const& bb{model.meshes[*n.mesh_index].bounding_box};
            n.aabb = calculate_aabb(bb, n.matrix);
        }

        n.matrix = ngnast::gltf::to_glm(fastgltf::getTransformMatrix(node));

        n.child_indices.reserve(node.children.size());
        std::ranges::copy(node.children, std::back_inserter(n.child_indices));

        for (size_t const i : n.child_indices)
        {
            load_node(asset, i, model);
        }

        model.nodes[node_index] = std::move(n);
    }

    void load_scenes(fastgltf::Asset const& asset, ngnast::scene_model_t& model)
    {
        for (fastgltf::Scene const& scene : asset.scenes)
        {
            ngnast::scene_graph_t s{.name = std::string{scene.name}};

            s.root_indices.reserve(scene.nodeIndices.size());
            std::ranges::copy(scene.nodeIndices,
                std::back_inserter(s.root_indices));

            model.scenes.push_back(std::move(s));

            for (size_t const node_index : scene.nodeIndices)
            {
                load_node(asset, node_index, model);
            }
        }
    }
} // namespace

ngnast::gltf::loader_t::loader_t(
    std::span<VkFormat> const& compressed_texture_formats)
    : compressed_texture_formats_{std::cbegin(compressed_texture_formats),
          std::cend(compressed_texture_formats)}
{
    if (!compressed_texture_formats.empty())
    {
        basist::basisu_transcoder_init();
    }
}

std::expected<ngnast::scene_model_t, std::error_code>
ngnast::gltf::loader_t::load(std::filesystem::path const& path)
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
        fastgltf::Extensions::KHR_materials_emissive_strength |
        fastgltf::Extensions::KHR_texture_basisu};

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
        spdlog::error("Failed to load asset: {}", asset.error());
        return std::unexpected{make_error_code(translate_error(data.error()))};
    }

    scene_model_t rv;

    try
    {
        load_samplers(asset.get(), rv);
        load_textures(asset.get(), rv);
        std::set<size_t> const unorm_images{load_materials(asset.get(), rv)};
        load_images(compressed_texture_formats_,
            parent_path,
            unorm_images,
            asset.get(),
            rv);

        rv.nodes.resize(asset->nodes.size());

        rv.meshes = load_meshes(asset.get(), rv.primitives);
        load_scenes(asset.get(), rv);
    }
    catch (std::exception const& ex)
    {
        spdlog::error("Failed to load model: {}", ex.what());
        return std::unexpected{make_error_code(error_t::unknown)};
    }

    return rv;
}
