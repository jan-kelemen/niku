#include <vkrndr_shader_module.hpp>

#include <vkrndr_debug_utils.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_utility.hpp>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

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

void vkrndr::destroy(device_t const* const device,
    shader_module_t const* const shader_module)
{
    if (device)
    {
        vkDestroyShaderModule(*device, shader_module->handle, nullptr);
    }
}

vkrndr::shader_module_t vkrndr::create_shader_module(device_t const& device,
    std::filesystem::path const& path,
    VkShaderStageFlagBits const stage,
    std::string_view const entry_point)
{
    std::vector<char> const code{read_file(path)};

    return create_shader_module(device,
        // NOLINTNEXTLINE(bugprone-bitwise-pointer-cast)
        std::span{std::bit_cast<uint32_t const*>(code.data()),
            code.size() / sizeof(uint32_t)},
        stage,
        entry_point);
}

vkrndr::shader_module_t vkrndr::create_shader_module(device_t const& device,
    std::span<uint32_t const> const& spirv,
    VkShaderStageFlagBits const stage,
    std::string_view const entry_point)
{
    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = spirv.size_bytes();
    create_info.pCode = spirv.data();

    // TODO-JK: maintenance5 feature - once it's supported by RenderDoc!!
    shader_module_t rv;
    rv.stage = stage;
    rv.entry_point = std::string{entry_point};
    check_result(
        vkCreateShaderModule(device, &create_info, nullptr, &rv.handle));

    return rv;
}

VkPipelineShaderStageCreateInfo vkrndr::as_pipeline_shader(
    shader_module_t const& shader,
    VkSpecializationInfo const* specialization)
{
    VkPipelineShaderStageCreateInfo rv{};
    rv.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    rv.stage = shader.stage;
    rv.module = shader.handle;
    rv.pName = shader.entry_point.c_str();
    rv.pSpecializationInfo = specialization;

    return rv;
}

void vkrndr::object_name(device_t const& device,
    shader_module_t const& shader_module,
    std::string_view name)
{
    object_name(device,
        VK_OBJECT_TYPE_SHADER_MODULE,
        std::bit_cast<uint64_t>(shader_module.handle),
        name);
}
