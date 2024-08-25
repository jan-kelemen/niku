#ifndef CPPEXT_ATTRIBUTE_INCLUDED
#define CPPEXT_ATTRIBUTE_INCLUDED

// clang-format off
#if defined(_MSC_VER)
#define CPPEXT_SUPPRESS

#elif defined(__GNUC__) && !defined(__clang__)
#define CPPEXT_SUPPRESS

#elif defined(__clang__)
#define CPPEXT_SUPPRESS [[clang::suppress]]

#else
#define CPPEXT_SUPPRESS

#endif
// clang-format on

#endif
