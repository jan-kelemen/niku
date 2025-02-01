#ifndef NGNAST_GLTF_LOADER_INCLUDED
#define NGNAST_GLTF_LOADER_INCLUDED

#include <ngnast_scene_model.hpp>

#include <expected>
#include <filesystem>
#include <system_error>

namespace ngnast::gltf
{
    class [[nodiscard]] loader_t final
    {
    public:
        [[nodiscard]] std::expected<scene_model_t, std::error_code> load(
            std::filesystem::path const& path);
    };
} // namespace ngnast::gltf

#endif
