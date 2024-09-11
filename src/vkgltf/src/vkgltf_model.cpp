#include <vkgltf_model.hpp>

#include <vkrndr_buffer.hpp>
#include <vkrndr_device.hpp>

void vkgltf::destroy(vkrndr::device_t* const device, model_t* const model)
{
    if (model)
    {
        destroy(device, &model->index_buffer);
        destroy(device, &model->vertex_buffer);
    }
}
