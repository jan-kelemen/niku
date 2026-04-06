#ifndef CPPEXT_HASH_ADAPTER_INCLUDED
#define CPPEXT_HASH_ADAPTER_INCLUDED

#include <boost/hash2/get_integral_result.hpp>
#include <boost/hash2/hash_append.hpp>

#include <cstddef>

namespace cppext
{
    template<class T, class H>
    struct [[nodiscard]] hash_adapter_t final
    {
        [[nodiscard]] size_t operator()(T const& v) const
        {
            H h;
            boost::hash2::hash_append(h, {}, v);
            return boost::hash2::get_integral_result<size_t>(h);
        }
    };
} // namespace cppext

#endif
