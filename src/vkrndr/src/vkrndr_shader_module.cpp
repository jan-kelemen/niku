#include <vkrndr_shader_module.hpp>

#include <vkrndr_debug_utils.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_utility.hpp>

#include <cppext_read_file.hpp>

#include <bit>
#include <cstdint>
#include <filesystem>
#include <vector>

void vkrndr::destroy(device_t const& device,
    shader_module_t const& shader_module)
{
    vkDestroyShaderModule(device, shader_module.handle, nullptr);
}

vkrndr::shader_module_t vkrndr::create_shader_module(device_t const& device,
    std::filesystem::path const& path,
    VkShaderStageFlagBits const stage,
    std::string_view const entry_point)
{
    std::vector<char> const code{cppext::read_file(path)};

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
    return {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = shader.stage,
        .module = shader.handle,
        .pName = shader.entry_point.c_str(),
        .pSpecializationInfo = specialization};
}

void vkrndr::object_name(device_t const& device,
    shader_module_t const& shader_module,
    std::string_view const name)
{
    object_name(device,
        VK_OBJECT_TYPE_SHADER_MODULE,
        handle_cast(shader_module.handle),
        name);
}
