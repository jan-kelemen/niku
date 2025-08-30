#ifndef HEATX_CONFIG_INCLUDED
#define HEATX_CONFIG_INCLUDED

namespace heatx
{
// NOLINTBEGIN(misc-redundant-expression)
#ifndef HEATX_SHADER_DEBUG_SYMBOLS
#define HEATX_SHADER_DEBUG_SYMBOLS 0
#endif
    constexpr bool enable_shader_debug_symbols{HEATX_SHADER_DEBUG_SYMBOLS != 0};

#ifndef HEATX_SHADER_OPTIMIZATION
#define HEATX_SHADER_OPTIMIZATION 0
#endif
    constexpr bool enable_shader_optimization{HEATX_SHADER_OPTIMIZATION != 0};

#ifndef HEATX_VALIDATION_LAYERS
#define HEATX_VALIDATION_LAYERS 0
#endif
    constexpr bool enable_validation_layers{HEATX_VALIDATION_LAYERS != 0};
    // NOLINTEND(misc-redundant-expression)
} // namespace heatx

#endif
