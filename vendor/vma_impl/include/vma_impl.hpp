#if defined(_MSC_VER)
#pragma push_macro("WIN32_LEAN_AND_MEAN")
#pragma push_macro("VC_EXTRALEAN")
#pragma push_macro("NOMINMAX")

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#define NOMINMAX
#endif

#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vk_mem_alloc.h> // IWYU pragma: export

#if defined(_MSC_VER)
#pragma pop_macro("WIN32_LEAN_AND_MEAN")
#pragma pop_macro("VC_EXTRALEAN")
#pragma pop_macro("NOMINMAX")
#endif
