#ifndef VKGLSL_SHADER_SET_INCLUDED
#define VKGLSL_SHADER_SET_INCLUDED

#include <volk.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

namespace vkglsl
{
    class [[nodiscard]] shader_set_t final
    {
    public:
        shader_set_t(bool with_debug_info = false, bool optimize = true);

        shader_set_t(shader_set_t const&) = delete;

        shader_set_t(shader_set_t&&) noexcept;

    public:
        ~shader_set_t();

    public:
        [[nodiscard]] bool add_include_directory(
            std::filesystem::path const& path);

        [[nodiscard]] bool add_shader(VkShaderStageFlagBits stage,
            std::filesystem::path const& file);

        [[nodiscard]] std::vector<uint32_t>* shader_binary(
            VkShaderStageFlagBits stage);

    public:
        shader_set_t& operator=(shader_set_t const&) = delete;

        shader_set_t& operator=(shader_set_t&&) noexcept;

    private:
        struct impl_t;
        std::unique_ptr<impl_t> impl_;
    };
} // namespace vkglsl

#endif
