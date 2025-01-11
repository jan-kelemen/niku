#ifndef NGNSCR_SCRIPT_COMPILER_INCLUDED
#define NGNSCR_SCRIPT_COMPILER_INCLUDED

#include <filesystem>
#include <memory>

class CScriptBuilder;

namespace ngnscr
{
    class scripting_engine_t;
} // namespace ngnscr

namespace ngnscr
{
    class [[nodiscard]] script_compiler_t final
    {
    public:
        explicit script_compiler_t(scripting_engine_t& engine);

        script_compiler_t(script_compiler_t const&) = delete;

        script_compiler_t(script_compiler_t&& other) noexcept;

    public:
        ~script_compiler_t();

    public:
        [[nodiscard]] bool new_module(char const* name);

        [[nodiscard]] bool add_section(std::filesystem::path const& path);

        [[nodiscard]] bool build();

    public:
        script_compiler_t& operator=(script_compiler_t const&) = delete;

        script_compiler_t& operator=(script_compiler_t&& other) noexcept;

    private:
        scripting_engine_t* engine_;
        std::unique_ptr<CScriptBuilder> builder_;
    };
} // namespace ngnscr

#endif
