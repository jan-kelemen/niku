#ifndef RESHED_CONFIG_INCLUDED
#define RESHED_CONFIG_INCLUDED

namespace reshed
{
#ifndef RESHED_SHADER_DEBUG_SYMBOLS
#define RESHED_SHADER_DEBUG_SYMBOLS 0
#endif
    constexpr bool enable_shader_debug_symbols{
        RESHED_SHADER_DEBUG_SYMBOLS != 0};

#ifndef RESHED_SHADER_OPTIMIZATION
#define RESHED_SHADER_OPTIMIZATION 0
#endif
    constexpr bool enable_shader_optimization{RESHED_SHADER_OPTIMIZATION != 0};

#ifndef RESHED_VALIDATION_LAYERS
#define RESHED_VALIDATION_LAYERS 0
#endif
    constexpr bool enable_validation_layers{RESHED_VALIDATION_LAYERS != 0};
} // namespace reshed

#endif
