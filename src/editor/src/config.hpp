#ifndef EDITOR_CONFIG_INCLUDED
#define EDITOR_CONFIG_INCLUDED

namespace editor
{
    // NOLINTBEGIN(misc-redundant-expression)
#ifndef EDITOR_SHADER_DEBUG_SYMBOLS
#define EDITOR_SHADER_DEBUG_SYMBOLS 0
#endif
    constexpr bool enable_shader_debug_symbols{
        EDITOR_SHADER_DEBUG_SYMBOLS != 0};

#ifndef EDITOR_SHADER_OPTIMIZATION
#define EDITOR_SHADER_OPTIMIZATION 0
#endif
    constexpr bool enable_shader_optimization{EDITOR_SHADER_OPTIMIZATION != 0};
    // NOLINTEND(misc-redundant-expression)
} // namespace editor

#endif
