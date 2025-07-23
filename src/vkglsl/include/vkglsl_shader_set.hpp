#ifndef VKGLSL_SHADER_SET_INCLUDED
#define VKGLSL_SHADER_SET_INCLUDED

#include <vkrndr_shader_module.hpp>

#include <volk.h>

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <span>
#include <string_view>
#include <system_error>
#include <vector>

namespace vkrndr
{
    struct device_t;
} // namespace vkrndr

namespace vkglsl
{
    class [[nodiscard]] shader_set_t final
    {
    public:
        explicit shader_set_t(bool with_debug_info = false,
            bool optimize = true);

        shader_set_t(shader_set_t const&) = delete;

        shader_set_t(shader_set_t&&) noexcept;

    public:
        ~shader_set_t();

    public:
        [[nodiscard]] bool add_include_directory(
            std::filesystem::path const& path);

        [[nodiscard]] std::expected<void, std::error_code> add_shader(
            VkShaderStageFlagBits stage,
            std::filesystem::path const& file,
            std::span<std::string_view const> const& preprocessor_defines = {},
            std::string_view entry_point = "main");

        [[nodiscard]] std::expected<void, std::error_code> add_shader_binary(
            VkShaderStageFlagBits stage,
            std::span<uint32_t const> const& binary,
            std::string_view entry_point = "main");

        [[nodiscard]] std::vector<uint32_t>* shader_binary(
            VkShaderStageFlagBits stage);

        [[nodiscard]] std::expected<vkrndr::shader_module_t, std::error_code>
        shader_module(vkrndr::device_t const& device,
            VkShaderStageFlagBits stage) const;

        [[nodiscard]] std::expected<std::vector<VkDescriptorSetLayoutBinding>,
            std::error_code>
        descriptor_bindings(uint32_t set) const;

    public:
        shader_set_t& operator=(shader_set_t const&) = delete;

        shader_set_t& operator=(shader_set_t&&) noexcept;

    private:
        struct impl_t;
        std::unique_ptr<impl_t> impl_;
    };

    [[nodiscard]] std::expected<vkrndr::shader_module_t, std::error_code>
    add_shader_module_from_path(shader_set_t& shader_set,
        vkrndr::device_t const& device,
        VkShaderStageFlagBits stage,
        std::filesystem::path const& file,
        std::span<std::string_view const> const& preprocessor_defines = {},
        std::string_view entry_point = "main");

    [[nodiscard]] std::expected<vkrndr::shader_module_t, std::error_code>
    add_shader_binary_from_path(shader_set_t& shader_set,
        vkrndr::device_t const& device,
        VkShaderStageFlagBits stage,
        std::filesystem::path const& file,
        std::string_view entry_point = "main");

    [[nodiscard]] std::expected<VkDescriptorSetLayout, std::error_code>
    descriptor_set_layout(shader_set_t const& shader_set,
        vkrndr::device_t const& device,
        uint32_t set);
} // namespace vkglsl

#endif
