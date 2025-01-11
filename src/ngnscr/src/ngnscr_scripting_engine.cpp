#include <ngnscr_scripting_engine.hpp>

#include <angelscript.h>
#include <scriptstdstring/scriptstdstring.h>

#include <spdlog/spdlog.h>

#include <cassert>
#include <utility>

// IWYU pragma: no_include <fmt/base.h>
// IWYU pragma: no_include <fmt/format.h>

namespace
{
    void message_callback(asSMessageInfo const* const msg,
        [[maybe_unused]] void* param)
    {
        if (msg->type == asMSGTYPE_INFORMATION)
        {
            spdlog::info("{} ({}, {}): {}",
                msg->section,
                msg->row,
                msg->col,
                msg->message);
        }
        else if (msg->type == asMSGTYPE_WARNING)
        {
            spdlog::warn("{} ({}, {}): {}",
                msg->section,
                msg->row,
                msg->col,
                msg->message);
        }
        else
        {
            spdlog::error("{} ({}, {}): {}",
                msg->section,
                msg->row,
                msg->col,
                msg->message);
        }
    }
} // namespace

ngnscr::scripting_engine_t::scripting_engine_t()
    : engine_{asCreateScriptEngine()}
{
    int const r{engine_->SetMessageCallback(asFUNCTION(message_callback),
        nullptr,
        asCALL_CDECL)};
    assert(r >= 0);

    RegisterStdString(engine_);
}

ngnscr::scripting_engine_t::scripting_engine_t(
    scripting_engine_t&& other) noexcept
    : engine_{std::exchange(other.engine_, nullptr)}
{
}

ngnscr::scripting_engine_t::~scripting_engine_t()
{
    if (engine_)
    {
        engine_->ShutDownAndRelease();
    }
}

asIScriptEngine& ngnscr::scripting_engine_t::engine() { return *engine_; }

std::unique_ptr<asIScriptContext, void (*)(asIScriptContext*)>
ngnscr::scripting_engine_t::execution_context(asIScriptFunction* const function)
{
    auto const deleter = [](asIScriptContext* const ctx)
    {
        if (ctx)
        {
            ctx->Release();
        }
    };

    std::unique_ptr<asIScriptContext, void (*)(asIScriptContext*)> rv{
        engine_->CreateContext(),
        deleter};

    if (auto const result{rv->Prepare(function)}; result < 0)
    {
        spdlog::error("Error {} preparing function {}",
            result,
            function ? function->GetName() : "NULL");
        return {nullptr, deleter};
    }

    return rv;
}

ngnscr::scripting_engine_t& ngnscr::scripting_engine_t::operator=(
    scripting_engine_t&& other) noexcept
{
    engine_ = std::exchange(other.engine_, nullptr);

    return *this;
}
