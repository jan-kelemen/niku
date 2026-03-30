#ifndef CPPEXT_HASH_INCLUDED
#define CPPEXT_HASH_INCLUDED

#include <optional>

namespace boost::hash2
{
    struct hash_append_tag;
} // namespace boost::hash2

// Enables std::optional to be hashable by boost::hash2.
//
// Can't really say I'm happy with extending the namespace of boost,
// but the expectation is to provide an overload in the namespace of the type.
// In this case it would be std::, making it a worse offense than extending the
// boost namespace...

namespace boost::hash2
{
    template<typename Provider, typename Hash, typename Flavor, class T>
    constexpr void tag_invoke(boost::hash2::hash_append_tag const& /* tag */,
        Provider const& pr,
        Hash& h,
        Flavor const& f,
        std::optional<T> const* v)
    {
        bool const initialized{v->has_value()};

        pr.hash_append(h, f, initialized);
        if (initialized)
        {
            pr.hash_append(h, f, v->value());
        }
    }
} // namespace boost::hash2

#endif
