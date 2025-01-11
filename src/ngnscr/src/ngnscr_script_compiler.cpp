#include <ngnscr_script_compiler.hpp>

#include <ngnscr_scripting_engine.hpp>

#include <scriptbuilder/scriptbuilder.h>

#include <fmt/std.h> // IWYU: pragma keep

#include <spdlog/spdlog.h>

ngnscr::script_compiler_t::script_compiler_t(scripting_engine_t& engine)
    : engine_{&engine}
    , builder_{std::make_unique<CScriptBuilder>()}
{
}

ngnscr::script_compiler_t::script_compiler_t(
    script_compiler_t&& other) noexcept = default;

ngnscr::script_compiler_t::~script_compiler_t() = default;

bool ngnscr::script_compiler_t::new_module(char const* const name)
{
    if (auto const result{builder_->StartNewModule(&engine_->engine(), name)};
        result < 0)
    {
        spdlog::error("Error {} starting new module {}", result, name);
        return false;
    }
    return true;
}

bool ngnscr::script_compiler_t::add_section(std::filesystem::path const& path)
{
    auto const abs{absolute(path)};

    if (auto const result{builder_->AddSectionFromFile(abs.string().c_str())};
        result < 0)
    {
        spdlog::error("Error {} adding section from path {}", result, abs);
        return false;
    }

    return true;
}

bool ngnscr::script_compiler_t::build()
{
    if (auto const result{builder_->BuildModule()}; result < 0)
    {
        spdlog::error("Error {} building module", result);
        return false;
    }

    return true;
}

ngnscr::script_compiler_t& ngnscr::script_compiler_t::operator=(
    script_compiler_t&& other) noexcept = default;
