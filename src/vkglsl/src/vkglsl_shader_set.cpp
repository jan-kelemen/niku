#include <optional>
#include <vkglsl_shader_set.hpp>

#include <vkglsl_glslang_adapter.hpp>

#include <cppext_numeric.hpp>

#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>

#include <spdlog/spdlog.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <list>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace glslang
{
    class TIntermediate;
} // namespace glslang

// IWYU pragma: no_include <fmt/base.h>

namespace
{
    [[nodiscard]] std::vector<char> read_file(std::filesystem::path const& file)
    {
        std::ifstream stream{file, std::ios::ate | std::ios::binary};

        if (!stream.is_open())
        {
            throw std::runtime_error{"failed to open file!"};
        }

        auto const eof{stream.tellg()};

        std::vector<char> buffer(static_cast<size_t>(eof));
        stream.seekg(0);

        stream.read(buffer.data(), eof);

        return buffer;
    }
} // namespace

struct [[nodiscard]] vkglsl::shader_set_t::impl_t final
{
    glslang::TProgram program;
    std::vector<std::unique_ptr<glslang::TShader>> shaders;
};

vkglsl::shader_set_t::shader_set_t() : impl_{std::make_unique<impl_t>()} { }

vkglsl::shader_set_t::shader_set_t(shader_set_t&&) noexcept = default;

vkglsl::shader_set_t::~shader_set_t() = default;

vkglsl::shader_set_t& vkglsl::shader_set_t::operator=(
    shader_set_t&&) noexcept = default;

bool vkglsl::shader_set_t::add_shader(VkShaderStageFlagBits const stage,
    std::filesystem::path const& file)
{
    EShLanguage const language{to_glslang(stage)};
    if (!impl_->program.getShaders(language).empty())
    {
        spdlog::error("Shader stage already exists");
        return false;
    }

    auto shader{std::make_unique<glslang::TShader>(language)};

    shader->setEnvInput(glslang::EShSourceGlsl,
        language,
        glslang::EShClientVulkan,
        100);
    shader->setEnvClient(glslang::EShClientVulkan,
        glslang::EShTargetVulkan_1_3);
    shader->setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_6);

    auto const path_str{file.string()};
    std::vector<char> const glsl_source{read_file(file)};

    std::array strings{glsl_source.data()};
    std::array lengths{cppext::narrow<int>(glsl_source.size())};
    std::array names{path_str.c_str()};
    shader->setStringsWithLengthsAndNames(strings.data(),
        lengths.data(),
        names.data(),
        1);

    if (!shader->parse(GetDefaultResources(), 100, true, EShMsgDefault))
    {
        spdlog::error("Shader compilation failed: {}", shader->getInfoLog());
        return false;
    }

    impl_->program.addShader(shader.get());

    impl_->shaders.push_back(std::move(shader));
    return true;
}

bool vkglsl::shader_set_t::build()
{
    if (!impl_->program.link(EShMsgDefault))
    {
        spdlog::error("Shader set linking failed: {}",
            impl_->program.getInfoLog());
        return false;
    }
    return true;
}

std::optional<std::vector<uint32_t>> vkglsl::shader_set_t::shader_binary(
    VkShaderStageFlagBits const stage)
{
    glslang::TIntermediate* const intermediate{
        impl_->program.getIntermediate(to_glslang(stage))};

    std::vector<uint32_t> rv;

    static_assert(std::is_same_v<uint32_t, unsigned int>);
    glslang::GlslangToSpv(*intermediate, rv);

    return std::make_optional(std::move(rv));
}
