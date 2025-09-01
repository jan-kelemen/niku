#ifndef VKRNDR_PIPELINE_LAYOUT_BUILDER_INCLUDED
#define VKRNDR_PIPELINE_LAYOUT_BUILDER_INCLUDED

#include <vkrndr_pipeline.hpp>

#include <volk.h>

#include <vector>

namespace vkrndr
{
    struct device_t;
} // namespace vkrndr

namespace vkrndr
{
    class [[nodiscard]] pipeline_layout_builder_t final
    {
    public:
        explicit pipeline_layout_builder_t(device_t const& device);

        pipeline_layout_builder_t(pipeline_layout_builder_t const&) = default;

        pipeline_layout_builder_t(
            pipeline_layout_builder_t&&) noexcept = default;

    public:
        ~pipeline_layout_builder_t() = default;

    public:
        pipeline_layout_t build();

        pipeline_layout_builder_t& add_descriptor_set_layout(
            VkDescriptorSetLayout descriptor_set_layout);

        pipeline_layout_builder_t& add_push_constants(
            VkPushConstantRange push_constant_range);

        template<typename T>
        pipeline_layout_builder_t&
        add_push_constants(VkShaderStageFlags stage_flags, uint32_t offset = 0);

    public:
        pipeline_layout_builder_t& operator=(
            pipeline_layout_builder_t const&) = default;

        pipeline_layout_builder_t& operator=(
            pipeline_layout_builder_t&&) = default;

    private:
        device_t const* device_;
        std::vector<VkDescriptorSetLayout> descriptor_set_layouts_;
        std::vector<VkPushConstantRange> push_constants_;
    };
} // namespace vkrndr

template<typename T>
vkrndr::pipeline_layout_builder_t&
vkrndr::pipeline_layout_builder_t::add_push_constants(
    VkShaderStageFlags const stage_flags,
    uint32_t const offset)
{
    // cppcheck-suppress-begin returnTempReference
    return add_push_constants(
        {.stageFlags = stage_flags, .offset = offset, .size = sizeof(T)});
    // cppcheck-suppress-end returnTempReference
}
#endif
