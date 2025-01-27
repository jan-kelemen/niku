#ifndef VKRNDR_SHADER_MODULE_INCLUDED
#define VKRNDR_SHADER_MODULE_INCLUDED

#include <volk.h>

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace vkrndr
{
    struct device_t;
} // namespace vkrndr

namespace vkrndr
{
    struct [[nodiscard]] shader_module_t final
    {
        VkShaderModule handle{VK_NULL_HANDLE};
        VkShaderStageFlagBits stage{};
        std::string entry_point;
    };

    void destroy(device_t* device, shader_module_t* shader_module);

    shader_module_t create_shader_module(device_t& device,
        std::filesystem::path const& path,
        VkShaderStageFlagBits stage,
        std::string_view entry_point);

    shader_module_t create_shader_module(device_t& device,
        std::span<uint32_t const> const& spirv,
        VkShaderStageFlagBits stage,
        std::string_view entry_point);

    [[nodiscard]] VkPipelineShaderStageCreateInfo as_pipeline_shader(
        shader_module_t const& shader,
        VkSpecializationInfo const* specialization = nullptr);

    void object_name(device_t const& device,
        shader_module_t const& shader_module,
        std::string_view name);
} // namespace vkrndr

#endif
