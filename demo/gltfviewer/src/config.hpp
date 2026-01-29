#ifndef GLTFVIEWER_CONFIG_INCLUDED
#define GLTFVIEWER_CONFIG_INCLUDED

namespace gltfviewer
{
// NOLINTBEGIN(misc-redundant-expression)
#ifndef GLTFVIEWER_SHADER_DEBUG_SYMBOLS
#define GLTFVIEWER_SHADER_DEBUG_SYMBOLS 0
#endif
    constexpr bool enable_shader_debug_symbols{
        GLTFVIEWER_SHADER_DEBUG_SYMBOLS != 0};

#ifndef GLTFVIEWER_SHADER_OPTIMIZATION
#define GLTFVIEWER_SHADER_OPTIMIZATION 0
#endif
    constexpr bool enable_shader_optimization{
        GLTFVIEWER_SHADER_OPTIMIZATION != 0};
} // namespace gltfviewer

#endif
