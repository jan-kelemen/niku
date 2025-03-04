#ifndef NGNPHY_JOLT_INCLUDED
#define NGNPHY_JOLT_INCLUDED

// Jolt.h is missing some an include for some of the symbols
// defined in the main Jolt header. This is resolved in a yet unreleased version
// as of 5.2.0

// clang-format off
#include <type_traits> // IWYU pragma: keep
#include <Jolt/Jolt.h> // IWYU pragma: export
// clang-format on

#endif
