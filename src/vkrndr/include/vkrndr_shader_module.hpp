#ifndef VKRNDR_SHADER_MODULE_INCLUDED
#define VKRNDR_SHADER_MODULE_INCLUDED

#include <volk.h>

#include <filesystem>
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

    [[nodiscard]] VkPipelineShaderStageCreateInfo as_pipeline_shader(
        shader_module_t const& shader,
        VkSpecializationInfo const* specialization = nullptr);
} // namespace vkrndr

#endif
