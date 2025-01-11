#ifndef NGNSCR_SCRIPTING_ENGINE_INCLUDED
#define NGNSCR_SCRIPTING_ENGINE_INCLUDED

#include <memory>

class asIScriptEngine;
class asIScriptContext;
class asIScriptFunction;

namespace ngnscr
{
    class [[nodiscard]] scripting_engine_t final
    {
    public:
        scripting_engine_t();

        scripting_engine_t(scripting_engine_t const&) = delete;

        scripting_engine_t(scripting_engine_t&& other) noexcept;

    public:
        ~scripting_engine_t();

    public:
        [[nodiscard]] asIScriptEngine& engine();

        [[nodiscard]] std::unique_ptr<asIScriptContext,
            void (*)(asIScriptContext*)>
        execution_context(asIScriptFunction* function);

    public:
        scripting_engine_t& operator=(scripting_engine_t const&) = default;

        scripting_engine_t& operator=(scripting_engine_t&& other) noexcept;

    private:
        asIScriptEngine* engine_;
    };
} // namespace ngnscr

#endif
