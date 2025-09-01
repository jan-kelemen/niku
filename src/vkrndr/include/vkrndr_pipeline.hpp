#ifndef VKRNDR_PIPELINE_INCLUDED
#define VKRNDR_PIPELINE_INCLUDED

#include <volk.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace vkrndr
{
    struct device_t;
} // namespace vkrndr

namespace vkrndr
{
    struct [[nodiscard]] pipeline_layout_t final
    {
        VkPipelineLayout layout{VK_NULL_HANDLE};

        [[nodiscard]] operator VkPipelineLayout() const noexcept;
    };

    void destroy(device_t const& device, pipeline_layout_t const& layout);

    struct [[nodiscard]] pipeline_t final
    {
        pipeline_layout_t layout;
        VkPipeline pipeline{VK_NULL_HANDLE};
        VkPipelineBindPoint type{VK_PIPELINE_BIND_POINT_GRAPHICS};

        [[nodiscard]] operator VkPipeline() const noexcept;
    };

    void destroy(device_t const& device, pipeline_t const& pipeline);

    void bind_pipeline(VkCommandBuffer command_buffer,
        pipeline_t const& pipeline,
        uint32_t first_set,
        std::span<VkDescriptorSet const> const& descriptor_sets);

    void bind_pipeline(VkCommandBuffer command_buffer,
        pipeline_t const& pipeline);

    void object_name(device_t const& device,
        pipeline_t const& pipeline,
        std::string_view name);
} // namespace vkrndr

inline vkrndr::pipeline_t::operator VkPipeline() const noexcept
{
    return pipeline;
}

inline vkrndr::pipeline_layout_t::operator VkPipelineLayout() const noexcept
{
    return layout;
}

inline void vkrndr::bind_pipeline(VkCommandBuffer command_buffer,
    pipeline_t const& pipeline)
{
    bind_pipeline(command_buffer, pipeline, 0, {});
}
#endif
