#include <vkrndr_pipeline.hpp>

#include <vkrndr_debug_utils.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_utility.hpp>

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstdint>
#include <iterator>
#include <memory>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

// IWYU pragma: no_include <type_traits>
// IWYU pragma: no_include <functional>

void vkrndr::bind_pipeline(VkCommandBuffer command_buffer,
    pipeline_t const& pipeline,
    uint32_t const first_set,
    std::span<VkDescriptorSet const> descriptor_sets)
{
    if (!descriptor_sets.empty())
    {
        vkCmdBindDescriptorSets(command_buffer,
            pipeline.type,
            *pipeline.layout,
            first_set,
            count_cast(descriptor_sets.size()),
            descriptor_sets.data(),
            0,
            nullptr);
    }

    vkCmdBindPipeline(command_buffer, pipeline.type, pipeline.pipeline);
}

void vkrndr::destroy(device_t const* const device, pipeline_t* const pipeline)
{
    if (pipeline)
    {
        vkDestroyPipeline(*device, pipeline->pipeline, nullptr);
        if (pipeline->layout.use_count() == 1)
        {
            vkDestroyPipelineLayout(*device, *pipeline->layout, nullptr);
        }
        pipeline->layout.reset();
    }
}

void vkrndr::object_name(device_t const& device,
    pipeline_t const& pipeline,
    std::string_view name)
{
    object_name(device,
        VK_OBJECT_TYPE_PIPELINE,
        std::bit_cast<uint64_t>(pipeline.pipeline),
        name);
}
