#ifndef CPPEXT_OVERLOADED_INCLUDED

namespace cppext
{
    template<class... Ts>
    struct overloaded : Ts...
    {
        using Ts::operator()...;
    };
} // namespace cppext

#endif
