#ifndef CPPEXT_OVERLOADED_INCLUDED

namespace cppext
{
    // NOLINTBEGIN(misc-multiple-inheritance)
    template<class... Ts>
    struct overloaded : Ts...
    {
        using Ts::operator()...;
    };

    // NOLINTEND(misc-multiple-inheritance)
} // namespace cppext

#endif
