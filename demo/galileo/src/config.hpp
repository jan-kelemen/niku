#ifndef GALILEO_CONFIG_INCLUDED
#define GALILEO_CONFIG_INCLUDED

namespace galileo
{
// NOLINTBEGIN(misc-redundant-expression)
#ifndef GALILEO_SHADER_DEBUG_SYMBOLS
#define GALILEO_SHADER_DEBUG_SYMBOLS 0
#endif
    constexpr bool enable_shader_debug_symbols{
        GALILEO_SHADER_DEBUG_SYMBOLS != 0};

#ifndef GALILEO_SHADER_OPTIMIZATION
#define GALILEO_SHADER_OPTIMIZATION 0
#endif
    constexpr bool enable_shader_optimization{GALILEO_SHADER_OPTIMIZATION != 0};
} // namespace galileo

#endif
