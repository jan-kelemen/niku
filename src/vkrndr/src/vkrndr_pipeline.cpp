#include <vkrndr_pipeline.hpp>

#include <vkrndr_debug_utils.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_utility.hpp>

#include <bit>
#include <cstdint>
#include <span>
#include <string_view>

void vkrndr::destroy(device_t const& device, pipeline_layout_t const& layout)
{
    vkDestroyPipelineLayout(device, layout, nullptr);
}

void vkrndr::bind_pipeline(VkCommandBuffer command_buffer,
    pipeline_t const& pipeline,
    uint32_t const first_set,
    std::span<VkDescriptorSet const> const& descriptor_sets)
{
    if (!descriptor_sets.empty())
    {
        vkCmdBindDescriptorSets(command_buffer,
            pipeline.type,
            pipeline.layout,
            first_set,
            count_cast(descriptor_sets),
            descriptor_sets.data(),
            0,
            nullptr);
    }

    vkCmdBindPipeline(command_buffer, pipeline.type, pipeline);
}

void vkrndr::destroy(device_t const& device, pipeline_t const& pipeline)
{
    vkDestroyPipeline(device, pipeline, nullptr);
}

void vkrndr::object_name(device_t const& device,
    pipeline_t const& pipeline,
    std::string_view const name)
{
    object_name(device,
        VK_OBJECT_TYPE_PIPELINE,
        std::bit_cast<uint64_t>(pipeline.handle),
        name);
}
