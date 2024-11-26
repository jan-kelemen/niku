#include <angelscript.h>
#include <scriptbuilder/scriptbuilder.h>
#include <scriptstdstring/scriptstdstring.h>

#include <fmt/format.h>

#include <spdlog/spdlog.h>

#include <cassert>
#include <string>

// IWYU pragma: no_include <fmt/base.h>

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

    void print(std::string const& msg) { fmt::print("{}", msg); }

} // namespace

int main()
{
    // Create the script engine
    asIScriptEngine* engine = asCreateScriptEngine();

    // Set the message callback to receive information on errors in human
    // readable form.
    int r = engine->SetMessageCallback(asFUNCTION(message_callback),
        nullptr,
        asCALL_CDECL);
    assert(r >= 0);

    // AngelScript doesn't have a built-in string type, as there is no definite
    // standard string type for C++ applications. Every developer is free to
    // register its own string type. The SDK do however provide a standard
    // add-on for registering a string type, so it's not necessary to implement
    // the registration yourself if you don't want to.
    RegisterStdString(engine);

    // Register the function that we want the scripts to call
    r = engine->RegisterGlobalFunction("void print(const string &in)",
        asFUNCTION(print),
        asCALL_CDECL);
    assert(r >= 0);

    // The CScriptBuilder helper is an add-on that loads the file,
    // performs a pre-processing pass if necessary, and then tells
    // the engine to build a script module.
    CScriptBuilder builder;
    r = builder.StartNewModule(engine, "MyModule");
    if (r < 0)
    {
        // If the code fails here it is usually because there
        // is no more memory to allocate the module
        spdlog::error("Unrecoverable error while starting a new module.");
        return -1;
    }
    r = builder.AddSectionFromFile("test.as");
    if (r < 0)
    {
        // The builder wasn't able to load the file. Maybe the file
        // has been removed, or the wrong name was given, or some
        // preprocessing commands are incorrectly written.
        spdlog::error("Please correct the errors in the script and try again.");
        return -1;
    }
    r = builder.BuildModule();
    if (r < 0)
    {
        // An error occurred. Instruct the script writer to fix the
        // compilation errors that were listed in the output stream.
        spdlog::error("Please correct the errors in the script and try again.");
        return -1;
    }

    // Find the function that is to be called.
    asIScriptModule* mod = engine->GetModule("MyModule");
    asIScriptFunction* func = mod->GetFunctionByDecl("void main()");
    if (!func)
    {
        // The function couldn't be found. Instruct the script writer
        // to include the expected function in the script.
        spdlog::error(
            "The script must have the function 'void main()'. Please add it and try again.");
        return -1;
    }

    // Create our context, prepare it, and then execute
    asIScriptContext* ctx = engine->CreateContext();
    ctx->Prepare(func);
    r = ctx->Execute();
    if (r != asEXECUTION_FINISHED)
    {
        // The execution didn't complete as expected. Determine what happened.
        if (r == asEXECUTION_EXCEPTION)
        {
            // An exception occurred, let the script writer know what happened
            // so it can be corrected.
            spdlog::error(
                "An exception '{}' occurred. Please correct the code and try again.",
                ctx->GetExceptionString());
        }
    }

    // Clean up
    ctx->Release();
    engine->ShutDownAndRelease();
}
