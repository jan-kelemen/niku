#ifndef CPPEXT_CONTAINER_INCLUDED
#define CPPEXT_CONTAINER_INCLUDED

#include <concepts>
#include <span>

namespace cppext
{
    template<typename T>
    [[nodiscard]] constexpr std::span<T> as_span(T& value)
    {
        return {&value, 1};
    }
} // namespace cppext

#endif
