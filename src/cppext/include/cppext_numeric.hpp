#ifndef CPPEXT_NUMERIC_INCLUDED
#define CPPEXT_NUMERIC_INCLUDED

#if defined(_MSC_VER) && !defined(__clang__)
#pragma push_macro("CPPEXT_NUMERIC_IS_MSVC")
#undef CPPEXT_NUMERIC_IS_MSVC
#define CPPEXT_NUMERIC_IS_MSVC
#endif

#ifndef __has_builtin
#define __has_builtin(__x) 0
#endif

#include <cassert>
#include <climits>
#include <concepts>
#include <limits>
#include <type_traits>
#include <utility>

#ifdef CPPEXT_NUMERIC_IS_MSVC
#include <intrin.h>
#include <softintrin.h>
#endif

namespace cppext
{
    namespace detail
    {
        template<typename T, typename U>
        [[nodiscard]] constexpr T* reinterpret_as_ptr(U* value)
        {
            static_assert(sizeof(T) == sizeof(U));
            static_assert(alignof(T) == alignof(U));
            static_assert(std::is_signed_v<T> == std::is_signed_v<U>);
            static_assert(std::is_signed_v<char>);

            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            return reinterpret_cast<T*>(value);
        }
    } // namespace detail

    template<std::integral To, std::integral From>
    [[nodiscard]] constexpr To narrow(From const value)
    {
        assert(std::in_range<To>(value));
        return static_cast<To>(value);
    }

    template<std::floating_point To = float>
    [[nodiscard]] constexpr To as_fp(auto const from)
    {
        return static_cast<To>(from);
    }

    template<std::integral T>
    [[nodiscard]] bool add(T lhs, T rhs, T& out) noexcept = delete;

    template<std::integral T>
    [[nodiscard]] bool sub(T lhs, T rhs, T& out) noexcept;

    template<std::integral T>
    [[nodiscard]] bool mul(T lhs, T rhs, T& out) noexcept;

    template<>
    [[nodiscard]] inline bool
    add(char const lhs, char const rhs, char& out) noexcept
    {
#if __has_builtin(__builtin_add_overflow)
        return !__builtin_add_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
#ifndef _CHAR_UNSIGNED
        return _add_overflow_i8(0,
                   lhs,
                   rhs,
                   detail::reinterpret_as_ptr<signed char>(&out)) == 0;
#else
        return _addcarry_u8(0,
                   lhs,
                   rhs,
                   detail::reinterpret_as_ptr<unsigned char>(&out)) == 0;
#endif
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool add(unsigned char const lhs,
        unsigned char const rhs,
        unsigned char& out) noexcept
    {
#if __has_builtin(__builtin_add_overflow)
        return !__builtin_add_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        return _addcarry_u8(0, lhs, rhs, &out) == 0;
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool
    add(signed char const lhs, signed char const rhs, signed char& out) noexcept
    {
#if __has_builtin(__builtin_add_overflow)
        return !__builtin_add_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        return _add_overflow_i8(0, lhs, rhs, &out) == 0;
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool
    add(char8_t const lhs, char8_t const rhs, char8_t& out) noexcept
    {
#if __has_builtin(__builtin_add_overflow)
        return !__builtin_add_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        return _addcarry_u8(0,
                   lhs,
                   rhs,
                   detail::reinterpret_as_ptr<unsigned char>(&out)) == 0;
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool
    add(char16_t const lhs, char16_t const rhs, char16_t& out) noexcept
    {
#if __has_builtin(__builtin_add_overflow)
        return !__builtin_add_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        return _addcarry_u16(0,
                   lhs,
                   rhs,
                   detail::reinterpret_as_ptr<unsigned short>(&out)) == 0;
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool
    add(char32_t const lhs, char32_t const rhs, char32_t& out) noexcept
    {
#if __has_builtin(__builtin_add_overflow)
        return !__builtin_add_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        return _addcarry_u32(0,
                   lhs,
                   rhs,
                   detail::reinterpret_as_ptr<unsigned int>(&out)) == 0;
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool
    add(wchar_t const lhs, wchar_t const rhs, wchar_t& out) noexcept
    {
#if __has_builtin(__builtin_add_overflow)
        return !__builtin_add_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        return _addcarry_u16(0,
                   lhs,
                   rhs,
                   detail::reinterpret_as_ptr<unsigned short>(&out)) == 0;
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool
    add(short const lhs, short const rhs, short& out) noexcept
    {
#if __has_builtin(__builtin_add_overflow)
        return !__builtin_add_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        return _add_overflow_i16(0, lhs, rhs, &out) == 0;
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool add(unsigned short const lhs,
        unsigned short const rhs,
        unsigned short& out) noexcept
    {
#if __has_builtin(__builtin_add_overflow)
        return !__builtin_add_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        return _addcarry_u16(0, lhs, rhs, &out) == 0;
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool
    add(int const lhs, int const rhs, int& out) noexcept
    {
#if __has_builtin(__builtin_add_overflow)
        return !__builtin_add_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        return _add_overflow_i32(0, lhs, rhs, &out) == 0;
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool add(unsigned int const lhs,
        unsigned int const rhs,
        unsigned int& out) noexcept
    {
#if __has_builtin(__builtin_add_overflow)
        return !__builtin_add_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        return _addcarry_u32(0, lhs, rhs, &out) == 0;
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool
    add(long const lhs, long const rhs, long& out) noexcept
    {
#if __has_builtin(__builtin_add_overflow)
        return !__builtin_add_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        return _add_overflow_i32(0,
                   lhs,
                   rhs,
                   detail::reinterpret_as_ptr<int>(&out)) == 0;
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool add(unsigned long const lhs,
        unsigned long const rhs,
        unsigned long& out) noexcept
    {
#if __has_builtin(__builtin_add_overflow)
        return !__builtin_add_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        return _addcarry_u32(0,
                   lhs,
                   rhs,
                   detail::reinterpret_as_ptr<unsigned int>(&out)) == 0;
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool
    add(long long const lhs, long long const rhs, long long& out) noexcept
    {
#if __has_builtin(__builtin_add_overflow)
        return !__builtin_add_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        return _add_overflow_i64(0, lhs, rhs, &out) == 0;
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool add(unsigned long long const lhs,
        unsigned long long const rhs,
        unsigned long long& out) noexcept
    {
#if __has_builtin(__builtin_add_overflow)
        return !__builtin_add_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        return _addcarry_u64(0, lhs, rhs, &out) == 0;
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool
    sub(char const lhs, char const rhs, char& out) noexcept
    {
#if __has_builtin(__builtin_sub_overflow)
        return !__builtin_sub_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
#ifndef _CHAR_UNSIGNED
        return _sub_overflow_i8(0,
                   lhs,
                   rhs,
                   detail::reinterpret_as_ptr<signed char>(&out)) == 0;
#else
        return _subborrow_u8(0,
                   lhs,
                   rhs,
                   detail::reinterpret_as_ptr<unsigned char>(&out)) == 0;
#endif
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool sub(unsigned char const lhs,
        unsigned char const rhs,
        unsigned char& out) noexcept
    {
#if __has_builtin(__builtin_sub_overflow)
        return !__builtin_sub_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        return _subborrow_u8(0, lhs, rhs, &out) == 0;
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool
    sub(signed char const lhs, signed char const rhs, signed char& out) noexcept
    {
#if __has_builtin(__builtin_sub_overflow)
        return !__builtin_sub_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        return _sub_overflow_i8(0, lhs, rhs, &out) == 0;
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool
    sub(char8_t const lhs, char8_t const rhs, char8_t& out) noexcept
    {
#if __has_builtin(__builtin_sub_overflow)
        return !__builtin_sub_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        return _subborrow_u8(0,
                   lhs,
                   rhs,
                   detail::reinterpret_as_ptr<unsigned char>(&out)) == 0;
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool
    sub(char16_t const lhs, char16_t const rhs, char16_t& out) noexcept
    {
#if __has_builtin(__builtin_sub_overflow)
        return !__builtin_sub_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        return _subborrow_u16(0,
                   lhs,
                   rhs,
                   detail::reinterpret_as_ptr<unsigned short>(&out)) == 0;
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool
    sub(char32_t const lhs, char32_t const rhs, char32_t& out) noexcept
    {
#if __has_builtin(__builtin_sub_overflow)
        return !__builtin_sub_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        return _subborrow_u32(0,
                   lhs,
                   rhs,
                   detail::reinterpret_as_ptr<unsigned int>(&out)) == 0;
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool
    sub(wchar_t const lhs, wchar_t const rhs, wchar_t& out) noexcept
    {
#if __has_builtin(__builtin_sub_overflow)
        return !__builtin_sub_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        return _subborrow_u16(0,
                   lhs,
                   rhs,
                   detail::reinterpret_as_ptr<unsigned short>(&out)) == 0;
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool
    sub(short const lhs, short const rhs, short& out) noexcept
    {
#if __has_builtin(__builtin_sub_overflow)
        return !__builtin_sub_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        return _sub_overflow_i16(0, lhs, rhs, &out) == 0;
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool sub(unsigned short const lhs,
        unsigned short const rhs,
        unsigned short& out) noexcept
    {
#if __has_builtin(__builtin_sub_overflow)
        return !__builtin_sub_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        return _subborrow_u16(0, lhs, rhs, &out) == 0;
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool
    sub(int const lhs, int const rhs, int& out) noexcept
    {
#if __has_builtin(__builtin_sub_overflow)
        return !__builtin_sub_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        return _sub_overflow_i32(0, lhs, rhs, &out) == 0;
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool sub(unsigned int const lhs,
        unsigned int const rhs,
        unsigned int& out) noexcept
    {
#if __has_builtin(__builtin_sub_overflow)
        return !__builtin_sub_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        return _subborrow_u32(0, lhs, rhs, &out) == 0;
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool
    sub(long const lhs, long const rhs, long& out) noexcept
    {
#if __has_builtin(__builtin_sub_overflow)
        return !__builtin_sub_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        return _sub_overflow_i32(0,
                   lhs,
                   rhs,
                   detail::reinterpret_as_ptr<int>(&out)) == 0;
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool sub(unsigned long const lhs,
        unsigned long const rhs,
        unsigned long& out) noexcept
    {
#if __has_builtin(__builtin_sub_overflow)
        return !__builtin_sub_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        return _subborrow_u32(0,
                   lhs,
                   rhs,
                   detail::reinterpret_as_ptr<unsigned int>(&out)) == 0;
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool
    sub(long long const lhs, long long const rhs, long long& out) noexcept
    {
#if __has_builtin(__builtin_sub_overflow)
        return !__builtin_sub_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        static_assert(std::is_same_v<long long, __int64>);
        return _sub_overflow_i64(0, lhs, rhs, &out) == 0;
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool sub(unsigned long long const lhs,
        unsigned long long const rhs,
        unsigned long long& out) noexcept
    {
#if __has_builtin(__builtin_sub_overflow)
        return !__builtin_sub_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        static_assert(std::is_same_v<unsigned long long, unsigned __int64>);
        return _subborrow_u64(0, lhs, rhs, &out) == 0;
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool
    mul(char const lhs, char const rhs, char& out) noexcept
    {
#if __has_builtin(__builtin_mul_overflow)
        return !__builtin_mul_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
#ifndef _CHAR_UNSIGNED
        signed short temp; // NOLINT
        if (_mul_full_overflow_i8(lhs, rhs, &temp) != 0) [[unlikely]]
        {
            return false;
        }
        out = static_cast<char>(temp);
        return true;
#else
        unsigned short temp; // NOLINT
        if (_mul_full_overflow_u8(lhs, rhs, &temp) != 0) [[unlikely]]
        {
            return false;
        }
        out = static_cast<char>(temp);
        return true;
#endif
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool mul(unsigned char const lhs,
        unsigned char const rhs,
        unsigned char& out) noexcept
    {
#if __has_builtin(__builtin_mul_overflow)
        return !__builtin_mul_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        unsigned short temp; // NOLINT
        if (_mul_full_overflow_u8(lhs, rhs, &temp) != 0) [[unlikely]]
        {
            return false;
        }
        out = static_cast<unsigned char>(temp);
        return true;
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool
    mul(signed char const lhs, signed char const rhs, signed char& out) noexcept
    {
#if __has_builtin(__builtin_mul_overflow)
        return !__builtin_mul_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        signed short temp; // NOLINT
        if (_mul_full_overflow_i8(lhs, rhs, &temp) != 0) [[unlikely]]
        {
            return false;
        }
        out = static_cast<signed char>(temp);
        return true;
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool
    mul(char8_t const lhs, char8_t const rhs, char8_t& out) noexcept
    {
#if __has_builtin(__builtin_mul_overflow)
        return !__builtin_mul_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        unsigned short temp; // NOLINT
        if (_mul_full_overflow_u8(lhs, rhs, &temp) != 0 || temp & 0xFF'00)
            [[unlikely]]
        {
            return false;
        }
        out = static_cast<char8_t>(temp);
        return true;
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool
    mul(char16_t const lhs, char16_t const rhs, char16_t& out) noexcept
    {
#if __has_builtin(__builtin_mul_overflow)
        return !__builtin_mul_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        unsigned short upper; // NOLINT
        return _mul_full_overflow_u16(lhs,
                   rhs,
                   detail::reinterpret_as_ptr<unsigned short>(&out),
                   &upper) == 0 &&
            upper == 0;
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool
    mul(char32_t const lhs, char32_t const rhs, char32_t& out) noexcept
    {
#if __has_builtin(__builtin_mul_overflow)
        return !__builtin_mul_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        unsigned int upper; // NOLINT
        return _mul_full_overflow_u32(lhs,
                   rhs,
                   detail::reinterpret_as_ptr<unsigned int>(&out),
                   &upper) == 0 &&
            upper == 0;
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool
    mul(wchar_t const lhs, wchar_t const rhs, wchar_t& out) noexcept
    {
#if __has_builtin(__builtin_mul_overflow)
        return !__builtin_mul_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        unsigned short upper; // NOLINT
        return _mul_full_overflow_u16(lhs,
                   rhs,
                   detail::reinterpret_as_ptr<unsigned short>(&out),
                   &upper) == 0 &&
            upper == 0;
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool
    mul(short const lhs, short const rhs, short& out) noexcept
    {
#if __has_builtin(__builtin_mul_overflow)
        return !__builtin_mul_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        return _mul_overflow_i16(lhs, rhs, &out) == 0;
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool mul(unsigned short const lhs,
        unsigned short const rhs,
        unsigned short& out) noexcept
    {
#if __has_builtin(__builtin_mul_overflow)
        return !__builtin_mul_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        unsigned short upper; // NOLINT
        return _mul_full_overflow_u16(lhs, rhs, &out, &upper) == 0 &&
            upper == 0;
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool
    mul(int const lhs, int const rhs, int& out) noexcept
    {
#if __has_builtin(__builtin_mul_overflow)
        return !__builtin_mul_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        return _mul_overflow_i32(lhs, rhs, &out) == 0;
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool mul(unsigned int const lhs,
        unsigned int const rhs,
        unsigned int& out) noexcept
    {
#if __has_builtin(__builtin_mul_overflow)
        return !__builtin_mul_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        unsigned int upper; // NOLINT
        return _mul_full_overflow_u32(lhs, rhs, &out, &upper) == 0 &&
            upper == 0;
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool
    mul(long const lhs, long const rhs, long& out) noexcept
    {
#if __has_builtin(__builtin_mul_overflow)
        return !__builtin_mul_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        return _mul_overflow_i32(lhs,
                   rhs,
                   detail::reinterpret_as_ptr<int>(&out)) == 0;
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool mul(unsigned long const lhs,
        unsigned long const rhs,
        unsigned long& out) noexcept
    {
#if __has_builtin(__builtin_mul_overflow)
        return !__builtin_mul_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        unsigned int upper; // NOLINT
        return _mul_full_overflow_u32(lhs,
                   rhs,
                   detail::reinterpret_as_ptr<unsigned int>(&out),
                   &upper) == 0 &&
            upper == 0;
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool
    mul(long long const lhs, long long const rhs, long long& out) noexcept
    {
#if __has_builtin(__builtin_mul_overflow)
        return !__builtin_mul_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        static_assert(std::is_same_v<long long, __int64>);
        return _mul_overflow_i64(lhs, rhs, &out) == 0;
#else
#error "Unsupported compiler"
#endif
    }

    template<>
    [[nodiscard]] inline bool mul(unsigned long long const lhs,
        unsigned long long const rhs,
        unsigned long long& out) noexcept
    {
#if __has_builtin(__builtin_mul_overflow)
        return !__builtin_mul_overflow(lhs, rhs, &out);
#elif defined(CPPEXT_NUMERIC_IS_MSVC)
        unsigned __int64 upper; // NOLINT
        return _mul_full_overflow_u64(lhs,
                   rhs,
                   detail::reinterpret_as_ptr<unsigned __int64>(&out),
                   &upper) == 0 &&
            upper == 0;
#else
#error "Unsupported compiler"
#endif
    }

    template<std::integral T>
    [[nodiscard]] bool div(T const lhs, T const rhs, T& out) noexcept
    {
        if constexpr (std::is_signed_v<T>)
        {
            if (rhs == T{} ||
                (lhs == std::numeric_limits<T>::min() && rhs == T{-1}))
                [[unlikely]]
            {
                return false;
            }
        }
        else
        {
            if (rhs == T{}) [[unlikely]]
            {
                return false;
            }
        }

        out = lhs / rhs;
        return true;
    }
} // namespace cppext

#if defined(_MSC_VER) && !defined(__clang__)
#pragma pop_macro("CPPEXT_NUMERIC_IS_MSVC")
#endif

#endif
