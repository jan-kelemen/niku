#include <pyramid_blur.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_debug_utils.hpp>
#include <vkrndr_device.hpp>
#include <vkrndr_utility.hpp>

gltfviewer::pyramid_blur_t::pyramid_blur_t(vkrndr::backend_t& backend)
    : backend_{&backend}
{
}

gltfviewer::pyramid_blur_t::~pyramid_blur_t()
{
    for (VkImageView view : mip_views_)
    {
        vkDestroyImageView(backend_->device().logical, view, nullptr);
    }

    destroy(&backend_->device(), &pyramid_image_);
}

vkrndr::image_t gltfviewer::pyramid_blur_t::source_image() const
{
    auto rv{pyramid_image_};
    rv.view = mip_views_.front();
    return rv;
}

void gltfviewer::pyramid_blur_t::resize(uint32_t const width,
    uint32_t const height)
{
    VkExtent2D const half_extent{width / 2, height / 2};

    destroy(&backend_->device(), &pyramid_image_);
    pyramid_image_ = vkrndr::create_image(backend_->device(),
        half_extent,
        vkrndr::max_mip_levels(half_extent.width, half_extent.height),
        VK_SAMPLE_COUNT_1_BIT,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    for (VkImageView view : mip_views_)
    {
        vkDestroyImageView(backend_->device().logical, view, nullptr);
    }
    mip_views_.resize(pyramid_image_.mip_levels);
    for (uint32_t i{}; i != pyramid_image_.mip_levels; ++i)
    {
        mip_views_[i] = vkrndr::create_image_view(backend_->device(),
            pyramid_image_.image,
            pyramid_image_.format,
            VK_IMAGE_ASPECT_COLOR_BIT,
            1,
            i);
    }

    object_name(backend_->device(), pyramid_image_, "Pyramid Image");
}
