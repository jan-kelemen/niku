#include <ngnast_error.hpp>

#include <cassert>
#include <string>

namespace
{
    struct [[nodiscard]] ngnast_error_category_t final : std::error_category
    {
        [[nodiscard]] char const* name() const noexcept override
        {
            return "ngnast";
        }

        [[nodiscard]] std::string message(int condition) const override
        {
            switch (static_cast<ngnast::error_t>(condition))
            {
            case ngnast::error_t::none:
                return "none";
            case ngnast::error_t::invalid_file:
                return "unable to open file";
            case ngnast::error_t::parse_failed:
                return "invalid file format";
            case ngnast::error_t::out_of_memory:
                return "out of memory";
            case ngnast::error_t::export_failed:
                return "export failed";
            case ngnast::error_t::load_transform_failed:
                return "load transformation failed";
            case ngnast::error_t::unknown:
                return "unknown";
            default:
                assert(false);
                return "unrecognized ngnast error";
            }
        }
    };

    ngnast_error_category_t const category{};
} // namespace

std::error_code ngnast::make_error_code(ngnast::error_t const e)
{
    return {static_cast<int>(e), category};
}
