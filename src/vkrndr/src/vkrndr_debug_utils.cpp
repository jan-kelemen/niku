#include <vkrndr_debug_utils.hpp>

#include <vkrndr_buffer.hpp>
#include <vkrndr_cubemap.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_pipeline.hpp>
#include <vkrndr_shader_module.hpp>

#include <volk.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <span>
#include <string_view>

vkrndr::command_buffer_scope_t::command_buffer_scope_t(
    VkCommandBuffer const command_buffer,
    std::string_view const label,
    std::span<float const, 3> const& color)
    : command_buffer_scope_t{command_buffer,
          label,
          std::array{color[0], color[1], color[2], 1.0f}}
{
}

vkrndr::command_buffer_scope_t::command_buffer_scope_t(
    VkCommandBuffer command_buffer,
    std::string_view label,
    std::span<float const, 4> const& color)
    : command_buffer_{command_buffer}
{
    if (!vkCmdBeginDebugUtilsLabelEXT)
    {
        return;
    }

    VkDebugUtilsLabelEXT vklabel{};
    vklabel.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    vklabel.pLabelName = label.data();
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    std::ranges::copy(color, vklabel.color);

    vkCmdBeginDebugUtilsLabelEXT(command_buffer, &vklabel);
}

vkrndr::command_buffer_scope_t::~command_buffer_scope_t() { close(); }

void vkrndr::command_buffer_scope_t::close() noexcept
{
    if (!vkCmdEndDebugUtilsLabelEXT || command_buffer_ == VK_NULL_HANDLE)
    {
        return;
    }

    vkCmdEndDebugUtilsLabelEXT(command_buffer_);

    command_buffer_ = VK_NULL_HANDLE;
}

void vkrndr::debug_label(VkCommandBuffer command_buffer,
    std::string_view label,
    std::span<float const, 3> const& color)
{
    debug_label(command_buffer,
        label,
        std::array{color[0], color[1], color[2], 0.0f});
}

void vkrndr::debug_label(VkCommandBuffer command_buffer,
    std::string_view label,
    std::span<float const, 4> const& color)
{
    if (!vkCmdInsertDebugUtilsLabelEXT)
    {
        return;
    }

    VkDebugUtilsLabelEXT vklabel{};
    vklabel.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    vklabel.pLabelName = label.data();

    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    std::ranges::copy(color, vklabel.color);

    vkCmdInsertDebugUtilsLabelEXT(command_buffer, &vklabel);
}

void vkrndr::object_name(VkDevice const device,
    VkObjectType const type,
    uint64_t const handle,
    std::string_view name)
{
    if (!vkSetDebugUtilsObjectNameEXT)
    {
        return;
    }

    VkDebugUtilsObjectNameInfoEXT const info{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = type,
        .objectHandle = handle,
        .pObjectName = name.data()};

    vkSetDebugUtilsObjectNameEXT(device, &info);
}
