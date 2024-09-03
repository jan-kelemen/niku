#ifndef VKGLTF_ERROR_INCLUDED
#define VKGLTF_ERROR_INCLUDED

#include <system_error>
#include <type_traits>

namespace vkgltf
{
    // NOLINTNEXTLINE(performance-enum-size)
    enum class error_t
    {
        none = 0,
        invalid_file,
        parse_failed,
        out_of_memory,
        export_failed,
        unknown
    };

    [[nodiscard]] std::error_code make_error_code(error_t e);
} // namespace vkgltf

template<>
struct std::is_error_code_enum<vkgltf::error_t> : std::true_type
{
};

#endif
