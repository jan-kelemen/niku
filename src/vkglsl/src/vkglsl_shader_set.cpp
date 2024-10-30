#include <vkglsl_shader_set.hpp>

#include <vkglsl_glslang_adapter.hpp>

#include <cppext_numeric.hpp>

#include <vkrndr_shader_module.hpp>

#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace glslang
{
    class TIntermediate;
} // namespace glslang

// IWYU pragma: no_include <fmt/base.h>
// IWYU pragma: no_include <span>

namespace
{
    struct [[nodiscard]] compiled_shader_t final
    {
        std::string entry_point;
        std::vector<uint32_t> code;
    };

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

    class [[nodiscard]] includer_t final : public glslang::TShader::Includer
    {
    public:
        includer_t() = default;

        includer_t(includer_t const&) = default;

        includer_t(includer_t&&) noexcept = default;

    public:
        ~includer_t() override = default;

    public:
        bool add_include_directory(std::filesystem::path const& path);

    public: // Includer overrides
        IncludeResult* includeSystem(char const* header_name,
            char const* includer_name,
            size_t depth) override;

        IncludeResult* includeLocal(char const* header_name,
            char const* includer_name,
            size_t depth) override;

        void releaseInclude(IncludeResult* result) override;

    public:
        includer_t& operator=(includer_t const&) = default;

        includer_t& operator=(includer_t&&) noexcept = default;

    private:
        struct [[nodiscard]] include_data_t final
        {
            std::vector<char> data;
        };

    private:
        [[nodiscard]] IncludeResult* result_for_path(
            std::filesystem::path const& path);

    private:
        std::vector<std::filesystem::path> include_directories_;
    };

    bool includer_t::add_include_directory(std::filesystem::path const& path)
    {
        auto abs{absolute(path)};
        if (std::ranges::find(include_directories_, abs) !=
            std::cend(include_directories_))
        {
            return true;
        }

        if (!exists(abs))
        {
            return false;
        }

        include_directories_.push_back(std::move(abs));
        return true;
    }

    glslang::TShader::Includer::IncludeResult* includer_t::includeSystem(
        [[maybe_unused]] char const* const header_name,
        [[maybe_unused]] char const* const includer_name,
        [[maybe_unused]] size_t const depth)
    {
        assert(false);
        return nullptr;
    }

    glslang::TShader::Includer::IncludeResult* includer_t::includeLocal(
        char const* const header_name,
        [[maybe_unused]] char const* const includer_name,
        [[maybe_unused]] size_t const depth)
    {
        auto const local_path{std::filesystem::current_path() / header_name};
        if (exists(local_path))
        {
            return result_for_path(local_path);
        }

        for (auto const& include_dir :
            include_directories_ | std::views::reverse)
        {
            auto const include_path{include_dir / header_name};
            if (exists(include_path))
            {
                return result_for_path(include_path);
            }
        }

        return nullptr;
    }

    void includer_t::releaseInclude(IncludeResult* const result)
    {
        if (result)
        {
            // NOLINTBEGIN(cppcoreguidelines-owning-memory)
            delete static_cast<include_data_t*>(result->userData);
            delete result;
            // NOLINTEND(cppcoreguidelines-owning-memory)
        }
    }

    glslang::TShader::Includer::IncludeResult* includer_t::result_for_path(
        std::filesystem::path const& path)
    {
        auto user_data{std::make_unique<include_data_t>(read_file(path))};
        auto rv{std::make_unique<IncludeResult>(path.string(),
            user_data->data.data(),
            user_data->data.size(),
            user_data.get())};

        // NOLINTNEXTLINE(bugprone-unused-return-value)
        static_cast<void>(user_data.release());

        return rv.release();
    }
} // namespace

struct [[nodiscard]] vkglsl::shader_set_t::impl_t final
{
    includer_t includer;
    std::map<EShLanguage, compiled_shader_t> shaders;
    bool with_debug_info{};
    bool optimize{};
};

vkglsl::shader_set_t::shader_set_t(bool const with_debug_info,
    bool const optimize)
    : impl_{std::make_unique<impl_t>()}
{
    impl_->with_debug_info = with_debug_info;
    impl_->optimize = optimize;
}

vkglsl::shader_set_t::shader_set_t(shader_set_t&&) noexcept = default;

vkglsl::shader_set_t::~shader_set_t() = default;

vkglsl::shader_set_t& vkglsl::shader_set_t::operator=(
    shader_set_t&&) noexcept = default;

bool vkglsl::shader_set_t::add_include_directory(
    std::filesystem::path const& path)
{
    return impl_->includer.add_include_directory(path);
}

bool vkglsl::shader_set_t::add_shader(VkShaderStageFlagBits const stage,
    std::filesystem::path const& file,
    std::string_view entry_point)
{
    EShLanguage const language{to_glslang(stage)};
    if (impl_->shaders.contains(language))
    {
        spdlog::error("Shader stage already exists");
        return false;
    }

    // NOLINTNEXTLINE(misc-const-correctness)
    std::string entry_point_str{entry_point};

    glslang::TShader shader{language};

    shader.setEnvInput(glslang::EShSourceGlsl,
        language,
        glslang::EShClientVulkan,
        100);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_3);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_6);
    shader.setDebugInfo(impl_->with_debug_info);

    auto const path_str{file.string()};
    std::vector<char> const glsl_source{read_file(file)};

    std::array const strings{glsl_source.data()};
    std::array const lengths{cppext::narrow<int>(glsl_source.size())};
    std::array const names{path_str.c_str()};
    shader.setStringsWithLengthsAndNames(strings.data(),
        lengths.data(),
        names.data(),
        1);
    shader.setEntryPoint(entry_point_str.c_str());
    shader.setSourceEntryPoint(entry_point_str.c_str());

    EShMessages messages{
        static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules)};
    if (impl_->with_debug_info)
    {
        messages = static_cast<EShMessages>(messages | EShMsgDebugInfo);
    }

    if (!shader.parse(GetDefaultResources(),
            460,
            EProfile::ECoreProfile,
            false,
            false,
            messages,
            impl_->includer))
    {
        spdlog::error("Shader compilation failed: {}\n{}\n",
            shader.getInfoLog(),
            shader.getInfoDebugLog());
        return false;
    }

    glslang::TProgram program;
    program.addShader(&shader);

    if (!program.link(EShMsgDefault))
    {
        spdlog::error("Shader linking failed: {}\n{}\n",
            program.getInfoLog(),
            program.getInfoDebugLog());
        return false;
    }

    glslang::TIntermediate* const intermediate{
        program.getIntermediate(language)};

    glslang::SpvOptions spv_options{};
    spv_options.generateDebugInfo = impl_->with_debug_info;
    spv_options.emitNonSemanticShaderDebugInfo = impl_->with_debug_info;
    spv_options.emitNonSemanticShaderDebugSource = impl_->with_debug_info;
    spv_options.disableOptimizer = impl_->optimize;

    std::vector<uint32_t> binary;
    static_assert(std::is_same_v<uint32_t, unsigned int>);
    glslang::GlslangToSpv(*intermediate, binary, &spv_options);

    impl_->shaders.emplace(std::piecewise_construct,
        std::forward_as_tuple(language),
        std::forward_as_tuple(std::move(entry_point_str), std::move(binary)));

    return true;
}

std::vector<uint32_t>* vkglsl::shader_set_t::shader_binary(
    VkShaderStageFlagBits const stage)
{
    if (auto it{impl_->shaders.find(to_glslang(stage))};
        it != std::cend(impl_->shaders))
    {
        return &it->second.code;
    }

    return nullptr;
}

tl::expected<vkrndr::shader_module_t, std::error_code>
vkglsl::shader_set_t::shader_module(vkrndr::device_t& device,
    VkShaderStageFlagBits const stage)
{
    if (auto it{impl_->shaders.find(to_glslang(stage))};
        it != std::cend(impl_->shaders))
    {
        return vkrndr::create_shader_module(device,
            it->second.code,
            stage,
            it->second.entry_point);
    }

    return tl::make_unexpected(
        std::make_error_code(std::errc::no_such_file_or_directory));
}
