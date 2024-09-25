#ifndef VKGLTF_LOADER_INCLUDED
#define VKGLTF_LOADER_INCLUDED

#include <vkgltf_model.hpp>

#include <tl/expected.hpp>

#include <filesystem>
#include <system_error>

namespace vkrndr
{
    class backend_t;
} // namespace vkrndr

namespace vkgltf
{
    class [[nodiscard]] loader_t final
    {
    public:
        explicit loader_t(vkrndr::backend_t* backend);

        loader_t(loader_t const&) = default;

        loader_t(loader_t&&) noexcept = default;

    public:
        ~loader_t() = default;

    public:
        [[nodiscard]] tl::expected<model_t, std::error_code> load(
            std::filesystem::path const& path);

    public:
        loader_t& operator=(loader_t const&) = default;

        loader_t& operator=(loader_t&&) noexcept = default;

    private:
        vkrndr::backend_t* backend_;
    };
} // namespace vkgltf

#endif
