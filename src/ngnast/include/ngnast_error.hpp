#ifndef NGNAST_ERROR_INCLUDED
#define NGNAST_ERROR_INCLUDED

#include <system_error>
#include <type_traits>

namespace ngnast
{
    enum class error_t
    {
        none = 0,
        invalid_file,
        parse_failed,
        out_of_memory,
        export_failed,
        load_transform_failed,
        unknown
    };

    [[nodiscard]] std::error_code make_error_code(error_t e);
} // namespace ngnast

template<>
struct std::is_error_code_enum<ngnast::error_t> : std::true_type
{
};

#endif
