#ifndef CPPEXT_PRAGMA_WARNING_INCLUDED
#define CPPEXT_PRAGMA_WARNING_INCLUDED

// https://www.fluentcpp.com/2019/08/30/how-to-disable-a-warning-in-cpp/

// clang-format off
#if defined(_MSC_VER)
#define DISABLE_WARNING_PUSH __pragma(warning(push))
#define DISABLE_WARNING_POP __pragma(warning(pop))
#define DISABLE_WARNING(warningNumber) \
    __pragma(warning(disable : warningNumber))

#define DISABLE_WARNING_MISSING_FIELD_INITIALIZERS
#define DISABLE_WARNING_USELESS_CAST
#define DISABLE_WARNING_CONVERSION
#define DISABLE_WARNING_STRUCTURE_WAS_PADDED_DUE_TO_ALIGNMENT_SPECIFIER \
    DISABLE_WARNING(4324)

#elif defined(__GNUC__) && !defined(__clang__)
#define DO_PRAGMA(X) _Pragma(#X)
#define DISABLE_WARNING_PUSH DO_PRAGMA(GCC diagnostic push)
#define DISABLE_WARNING_POP DO_PRAGMA(GCC diagnostic pop)
#define DISABLE_WARNING(warningName) \
    DO_PRAGMA(GCC diagnostic ignored #warningName)

#define DISABLE_WARNING_MISSING_FIELD_INITIALIZERS \
    DISABLE_WARNING(-Wmissing-field-initializers)
#define DISABLE_WARNING_USELESS_CAST DISABLE_WARNING(-Wuseless-cast)
#define DISABLE_WARNING_CONVERSION DISABLE_WARNING(-Wconversion)
#define DISABLE_WARNING_STRUCTURE_WAS_PADDED_DUE_TO_ALIGNMENT_SPECIFIER 

#elif defined(__clang__)
#define DO_PRAGMA(X) _Pragma(#X)
#define DISABLE_WARNING_PUSH DO_PRAGMA(GCC diagnostic push)
#define DISABLE_WARNING_POP DO_PRAGMA(GCC diagnostic pop)
#define DISABLE_WARNING(warningName) \
    DO_PRAGMA(GCC diagnostic ignored #warningName)

#define DISABLE_WARNING_MISSING_FIELD_INITIALIZERS \
    DISABLE_WARNING(-Wmissing-field-initializers)
#define DISABLE_WARNING_USELESS_CAST
#define DISABLE_WARNING_CONVERSION DISABLE_WARNING(-Wconversion)
#define DISABLE_WARNING_STRUCTURE_WAS_PADDED_DUE_TO_ALIGNMENT_SPECIFIER 

#else
#define DISABLE_WARNING_PUSH
#define DISABLE_WARNING_POP

#define DISABLE_WARNING_MISSING_FIELD_INITIALIZERS
#define DISABLE_WARNING_USELESS_CAST
#define DISABLE_WARNING_CONVERSION
#define DISABLE_WARNING_STRUCTURE_WAS_PADDED_DUE_TO_ALIGNMENT_SPECIFIER 
#endif
// clang-format on

#endif
