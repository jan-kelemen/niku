#include <gbuffer.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_commands.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_utility.hpp>

// IWYU pragma: no_include <initializer_list>

galileo::gbuffer_t::gbuffer_t(vkrndr::backend_t& backend) : backend_{&backend}
{
}

galileo::gbuffer_t::~gbuffer_t()
{
    destroy(&backend_->device(), &specular_image_);
    destroy(&backend_->device(), &albedo_image_);
    destroy(&backend_->device(), &normal_image_);
    destroy(&backend_->device(), &position_image_);
}

vkrndr::image_t& galileo::gbuffer_t::position_image()
{
    return position_image_;
}

vkrndr::image_t& galileo::gbuffer_t::normal_image() { return normal_image_; }

vkrndr::image_t& galileo::gbuffer_t::albedo_image() { return albedo_image_; }

vkrndr::image_t& galileo::gbuffer_t::specular_image()
{
    return specular_image_;
}

VkExtent2D galileo::gbuffer_t::extent() const { return position_image_.extent; }

void galileo::gbuffer_t::resize(uint32_t const width, uint32_t const height)
{
    auto const new_extent{vkrndr::to_extent(width, height)};

    destroy(&backend_->device(), &position_image_);
    position_image_ = vkrndr::create_image_and_view(backend_->device(),
        new_extent,
        1,
        VK_SAMPLE_COUNT_1_BIT,
        position_format,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT);

    destroy(&backend_->device(), &normal_image_);
    normal_image_ = vkrndr::create_image_and_view(backend_->device(),
        new_extent,
        1,
        VK_SAMPLE_COUNT_1_BIT,
        normal_format,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT);

    destroy(&backend_->device(), &albedo_image_);
    albedo_image_ = vkrndr::create_image_and_view(backend_->device(),
        new_extent,
        1,
        VK_SAMPLE_COUNT_1_BIT,
        albedo_format,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT);

    destroy(&backend_->device(), &specular_image_);
    specular_image_ = vkrndr::create_image_and_view(backend_->device(),
        new_extent,
        1,
        VK_SAMPLE_COUNT_1_BIT,
        specular_format,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT);
}

void galileo::gbuffer_t::transition(VkCommandBuffer command_buffer,
    VkImageLayout const old_layout,
    VkPipelineStageFlags2 const src_stage_mask,
    VkAccessFlags2 const src_access_mask,
    VkImageLayout const new_layout,
    VkPipelineStageFlags2 const dst_stage_mask,
    VkAccessFlags2 const dst_access_mask) const
{
    std::array<VkImageMemoryBarrier2, 4> barriers;

    auto it{std::begin(barriers)};

    for (auto const& image :
        {position_image_, normal_image_, albedo_image_, specular_image_})
    {
        *it = vkrndr::with_access(vkrndr::on_stage(vkrndr::image_barrier(image),
                                      src_stage_mask,
                                      dst_stage_mask),
            src_access_mask,
            dst_access_mask);

        if (old_layout != new_layout)
        {
            *it = vkrndr::with_layout(*it, old_layout, new_layout);
        }

        ++it;
    }

    vkrndr::wait_for(command_buffer, {}, {}, barriers);
}
