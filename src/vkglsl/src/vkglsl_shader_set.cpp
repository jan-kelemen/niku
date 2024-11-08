#include <vkglsl_shader_set.hpp>

#include <vkglsl_glslang_adapter.hpp>
#include <vkglsl_spirv_cross_adapter.hpp>

#include <cppext_numeric.hpp>
#include <cppext_pragma_warning.hpp>

#include <vkrndr_descriptors.hpp>
#include <vkrndr_shader_module.hpp>

#include <fmt/format.h>

#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>

#include <spdlog/spdlog.h>

#include <spirv_cross.hpp>

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
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace glslang
{
    class TIntermediate;
} // namespace glslang

// IWYU pragma: no_include <fmt/base.h>
// IWYU pragma: no_include <optional>

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

    [[nodiscard]] std::string preamble(
        std::span<std::string_view const> const& defines)
    {
        std::string rv;

        for (auto const& define : defines)
        {
            rv += fmt::format("#define {0}\n", define);
        }

        return rv;
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

tl::expected<void, std::error_code> vkglsl::shader_set_t::add_shader(
    VkShaderStageFlagBits const stage,
    std::filesystem::path const& file,
    std::span<std::string_view const> const& preprocessor_defines,
    std::string_view entry_point)
{
    EShLanguage const language{to_glslang(stage)};
    if (impl_->shaders.contains(language))
    {
        spdlog::error("Shader stage already exists");
        return tl::make_unexpected(
            std::make_error_code(std::errc::file_exists));
    }

    std::string const preamble_str{preamble(preprocessor_defines)};

    // NOLINTNEXTLINE(misc-const-correctness)
    std::string entry_point_str{entry_point};

    glslang::TShader shader{language};

    shader.setPreamble(preamble_str.c_str());

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
        return tl::make_unexpected(
            std::make_error_code(std::errc::executable_format_error));
    }

    glslang::TProgram program;
    program.addShader(&shader);

    if (!program.link(EShMsgDefault))
    {
        spdlog::error("Shader linking failed: {}\n{}\n",
            program.getInfoLog(),
            program.getInfoDebugLog());
        return tl::make_unexpected(
            std::make_error_code(std::errc::executable_format_error));
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

    return {};
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
    VkShaderStageFlagBits const stage) const
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

tl::expected<std::vector<VkDescriptorSetLayoutBinding>, std::error_code>
vkglsl::shader_set_t::descriptor_bindings(uint32_t const set) const
{
    std::vector<VkDescriptorSetLayoutBinding> rv;

    auto binding_for =
        [&rv](uint32_t const binding) mutable -> VkDescriptorSetLayoutBinding&
    {
        if (auto it{std::ranges::find(rv,
                binding,
                &VkDescriptorSetLayoutBinding::binding)};
            it != std::cend(rv))
        {
            return *it;
        }

        return rv.emplace_back(binding,
            VK_DESCRIPTOR_TYPE_MAX_ENUM,
            0,
            0,
            nullptr);
    };

    for (auto const& [lang, shader] : impl_->shaders)
    {
        spirv_cross::Compiler compiler{shader.code};
        auto const filter_descriptor_set =
            [&compiler, set](spirv_cross::Resource const& resource)
        {
            return resource_decoration(compiler,
                       resource,
                       spv::DecorationDescriptorSet) == set;
        };

        auto const declare_resources =
            [&](spirv_cross::SmallVector<spirv_cross::Resource> const&
                    resources,
                VkDescriptorType const type)
        {
            for (spirv_cross::Resource const& resource :
                std::views::filter(resources, filter_descriptor_set))
            {
                uint32_t const binding{compiler.get_decoration(resource.id,
                    spv::DecorationBinding)};

                auto& bind_point{binding_for(binding)};
                bind_point.descriptorType = type;
                bind_point.descriptorCount = 1;

                DISABLE_WARNING_PUSH
                DISABLE_WARNING_SIGN_CONVERSION
                bind_point.stageFlags |= to_vulkan(lang);
                DISABLE_WARNING_POP
            }
        };

        spirv_cross::ShaderResources const resources{
            compiler.get_shader_resources()};

        declare_resources(resources.sampled_images,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        declare_resources(resources.uniform_buffers,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        declare_resources(resources.storage_buffers,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        declare_resources(resources.separate_samplers,
            VK_DESCRIPTOR_TYPE_SAMPLER);
        declare_resources(resources.separate_images,
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
        declare_resources(resources.storage_images,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    }

    return rv;
}

tl::expected<vkrndr::shader_module_t, std::error_code>
vkglsl::add_shader_module_from_path(shader_set_t& shader_set,
    vkrndr::device_t& device,
    VkShaderStageFlagBits const stage,
    std::filesystem::path const& file,
    std::span<std::string_view const> const& preprocessor_defines,
    std::string_view entry_point)
{
    return shader_set.add_shader(stage, file, preprocessor_defines, entry_point)
        .and_then([&]() { return shader_set.shader_module(device, stage); });
}

tl::expected<VkDescriptorSetLayout, std::error_code>
vkglsl::descriptor_set_layout(shader_set_t const& shader_set,
    vkrndr::device_t const& device,
    uint32_t const set)
{
    return shader_set.descriptor_bindings(set).and_then(
        [&device](auto&& bindings)
        {
            return tl::expected<VkDescriptorSetLayout, std::error_code>{
                vkrndr::create_descriptor_set_layout(device, bindings)};
        });
}
