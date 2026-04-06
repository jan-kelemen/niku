#include <material_manager.hpp>

#include <vkrndr_device.hpp>
#include <vkrndr_image.hpp>

#include <volk.h>

#include <algorithm>
#include <system_error>

std::expected<editor::material_manager_t, std::error_code>
editor::create_material_manager([[maybe_unused]] vkrndr::device_t const& device)
{
    return {};
}

void editor::destroy(vkrndr::device_t const& device,
    material_manager_t const& materials)
{
    std::ranges::for_each(materials.samplers,
        [&device](VkSampler const& s)
        { vkDestroySampler(device, s, nullptr); });

    std::ranges::for_each(materials.images,
        [&device](vkrndr::image_t const& image) { destroy(device, image); });
}
