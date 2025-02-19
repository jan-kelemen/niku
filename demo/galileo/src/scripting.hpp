#ifndef GALILEO_SCRIPTING_INCLUDED
#define GALILEO_SCRIPTING_INCLUDED

class asIScriptFunction;

namespace galileo::component
{
    struct [[nodiscard]] scripts_t final
    {
        asIScriptFunction* on_character_hit_script{nullptr};
    };
} // namespace galileo::component

#endif
