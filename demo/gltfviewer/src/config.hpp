#ifndef GLTFVIEWER_CONFIG_INCLUDED
#define GLTFVIEWER_CONFIG_INCLUDED

namespace gltfviewer
{
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

#ifndef GLTFVIEWER_VALIDATION_LAYERS
#define GLTFVIEWER_VALIDATION_LAYERS 0
#endif
    constexpr bool enable_validation_layers{GLTFVIEWER_VALIDATION_LAYERS != 0};
} // namespace gltfviewer

#endif
