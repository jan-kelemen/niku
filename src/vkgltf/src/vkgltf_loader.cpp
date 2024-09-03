#include <fastgltf/types.hpp>
#include <vkgltf_loader.hpp>

#include <vkgltf_error.hpp>
#include <vkgltf_fastgltf_adapter.hpp>
#include <vkgltf_model.hpp>

#include <vkrndr_backend.hpp>

#include <fastgltf/core.hpp>

#include <spdlog/spdlog.h>

#include <tl/expected.hpp>

#include <system_error>

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

    auto asset{parser.loadGltf(data.get(), path.parent_path())};
    if (asset.error() != fastgltf::Error::None)
    {
        spdlog::error("Failed to load asset: {}", data.error());
        return tl::make_unexpected(
            make_error_code(translate_error(data.error())));
    }

    for (auto const& scene : asset->scenes)
    {
        spdlog::info("{}", scene.name);
    }

    return {};
}
