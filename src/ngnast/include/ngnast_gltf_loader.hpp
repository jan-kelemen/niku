#ifndef NGNAST_GLTF_LOADER_INCLUDED
#define NGNAST_GLTF_LOADER_INCLUDED

#include <ngnast_scene_model.hpp>

#include <expected>
#include <filesystem>
#include <system_error>

namespace vkrndr
{
    class backend_t;
} // namespace vkrndr

namespace ngnast::gltf
{
    class [[nodiscard]] loader_t final
    {
    public:
        explicit loader_t(vkrndr::backend_t& backend);

        loader_t(loader_t const&) = default;

        loader_t(loader_t&&) noexcept = default;

    public:
        ~loader_t() = default;

    public:
        [[nodiscard]] std::expected<scene_model_t, std::error_code> load(
            std::filesystem::path const& path);

    public:
        loader_t& operator=(loader_t const&) = default;

        loader_t& operator=(loader_t&&) noexcept = default;

    private:
        vkrndr::backend_t* backend_;
    };
} // namespace ngnast::gltf

#endif
