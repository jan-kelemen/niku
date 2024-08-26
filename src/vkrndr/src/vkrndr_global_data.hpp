#ifndef VKRNDR_GLOBAL_DATA_INCLUDED
#define VKRNDR_GLOBAL_DATA_INCLUDED

#include <atomic>

namespace vkrndr
{
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    extern std::atomic_bool swap_chain_refresh;
} // namespace vkrndr

#endif // !VKRNDR_GLOBAL_DATA_INCLUDED
