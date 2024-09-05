#include <vkgltf_model.hpp>

#include <cppext_numeric.hpp>

#include <vkrndr_device.hpp>
#include <vkrndr_image.hpp>

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <variant>

void vkgltf::destroy(vkrndr::device_t* const device, model_t* const model)
{
    if (model)
    {
        for (VkSampler sampler : model->samplers)
        {
            vkDestroySampler(device->logical, sampler, nullptr);
        }

        for (image_t image : model->images)
        {
            destroy(device, &image.source);
        }
    }
}

uint32_t vkgltf::index_buffer_t::count() const
{
    return std::visit([](auto const& buff)
        { return cppext::narrow<uint32_t>(buff.size()); },
        buffer);
}
