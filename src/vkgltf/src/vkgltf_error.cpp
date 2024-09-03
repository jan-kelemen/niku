#include <vkgltf_error.hpp>

#include <cassert>
#include <string>

namespace
{
    struct [[nodiscard]] vkgltf_error_category_t final : std::error_category
    {
        [[nodiscard]] char const* name() const noexcept override
        {
            return "vkgltf";
        }

        [[nodiscard]] std::string message(int condition) const override
        {
            switch (static_cast<vkgltf::error_t>(condition))
            {
            case vkgltf::error_t::none:
                return "none";
            case vkgltf::error_t::invalid_file:
                return "unable to open file";
            case vkgltf::error_t::parse_failed:
                return "invalid file format";
            case vkgltf::error_t::out_of_memory:
                return "out of memory";
            case vkgltf::error_t::export_failed:
                return "export failed";
            case vkgltf::error_t::unknown:
                return "unknown";
            default:
                assert(false);
                return "unrecognized vkgltf error";
            }
        }
    };

    vkgltf_error_category_t const category{};
} // namespace

std::error_code vkgltf::make_error_code(vkgltf::error_t const e)
{
    return {static_cast<int>(e), category};
}
