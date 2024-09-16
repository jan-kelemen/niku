#include <vkgltf_model.hpp>

#include <vkrndr_buffer.hpp>
#include <vkrndr_image.hpp>

void vkgltf::destroy(vkrndr::device_t* const device, model_t* const model)
{
    if (model)
    {
        for (auto& image : model->images)
        {
            destroy(device, &image);
        }

        destroy(device, &model->index_buffer);
        destroy(device, &model->vertex_buffer);
    }
}
