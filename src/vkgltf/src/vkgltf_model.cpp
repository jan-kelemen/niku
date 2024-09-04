#include <vkgltf_model.hpp>

#include <vkrndr_device.hpp>
#include <vkrndr_image.hpp>

#include <vulkan/vulkan_core.h>

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
