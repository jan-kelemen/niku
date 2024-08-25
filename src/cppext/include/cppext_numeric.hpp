#ifndef CPPEXT_NUMERIC_INCLUDED
#define CPPEXT_NUMERIC_INCLUDED

#include <cassert>
#include <concepts>
#include <utility>

namespace cppext
{
    template<std::integral To>
    [[nodiscard]] constexpr To narrow(std::integral auto const value)
    {
        assert(std::in_range<To>(value));
        return static_cast<To>(value);
    }

    template<std::floating_point To = float>
    [[nodiscard]] constexpr To as_fp(auto const from)
    {
        return static_cast<To>(from);
    }
} // namespace cppext

#endif
