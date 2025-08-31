#ifndef CPPEXT_MEMORY_INCLUDED
#define CPPEXT_MEMORY_INCLUDED

#include <memory>

namespace cppext
{
    template<typename T, void (*f)(T*)>
    struct [[nodiscard]] static_function_deleter_t final
    {
        void operator()(T* const p) const noexcept(noexcept(f(p))) { f(p); }
    };

    template<typename T, void (*f)(T*)>
    using unique_ptr_with_static_deleter_t =
        std::unique_ptr<T, static_function_deleter_t<T, f>>;

    template<typename T>
    using unique_ptr_with_deleter_t = std::unique_ptr<T, void (*)(T*)>;

    template<typename T>
    [[nodiscard]] constexpr T aligned_size(T value, T alignment);
} // namespace cppext

template<typename T>
constexpr T cppext::aligned_size(T const value, T const alignment)
{
    // https://stackoverflow.com/a/23928177
    return (value + alignment - 1) - value % alignment;
}

#endif
