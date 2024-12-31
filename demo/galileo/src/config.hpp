#ifndef GALILEO_CONFIG_INCLUDED
#define GALILEO_CONFIG_INCLUDED

namespace galileo
{
#ifndef GALILEO_SHADER_DEBUG_SYMBOLS
#define GALILEO_SHADER_DEBUG_SYMBOLS 0
#endif
    constexpr bool enable_shader_debug_symbols{
        GALILEO_SHADER_DEBUG_SYMBOLS != 0};

#ifndef GALILEO_SHADER_OPTIMIZATION
#define GALILEO_SHADER_OPTIMIZATION 0
#endif
    constexpr bool enable_shader_optimization{GALILEO_SHADER_OPTIMIZATION != 0};

#ifndef GALILEO_VALIDATION_LAYERS
#define GALILEO_VALIDATION_LAYERS 0
#endif
    constexpr bool enable_validation_layers{GALILEO_VALIDATION_LAYERS != 0};
} // namespace galileo

#endif
