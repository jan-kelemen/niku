#ifndef NGNAST_GLTF_LOADER_INCLUDED
#define NGNAST_GLTF_LOADER_INCLUDED

#include <ngnast_scene_model.hpp>

#include <volk.h>

#include <expected>
#include <filesystem>
#include <span>
#include <system_error>
#include <vector>

namespace ngnast::gltf
{
    class [[nodiscard]] loader_t final
    {
    public:
        loader_t(std::span<VkFormat> const& compressed_texture_formats);

    public:
        [[nodiscard]] std::expected<scene_model_t, std::error_code> load(
            std::filesystem::path const& path);

    private:
        std::vector<VkFormat> compressed_texture_formats_;
    };
} // namespace ngnast::gltf

#endif
