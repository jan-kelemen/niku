#include <gbuffer.hpp>

#include <ngngfx_gbuffer.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_debug_utils.hpp>
#include <vkrndr_image.hpp>
#include <vkrndr_synchronization.hpp>
#include <vkrndr_utility.hpp>

#include <vma_impl.hpp>

#include <array>
#include <iterator>
#include <vector>

// IWYU pragma: no_include <initializer_list>
// IWYU pragma: no_include <span>

galileo::gbuffer_t::gbuffer_t(vkrndr::backend_t& backend) : backend_{&backend}
{
}

galileo::gbuffer_t::~gbuffer_t() { destroy(&backend_->device(), &gbuffer_); }

vkrndr::image_t& galileo::gbuffer_t::position_image()
{
    return gbuffer_.images[0];
}

vkrndr::image_t& galileo::gbuffer_t::normal_image()
{
    return gbuffer_.images[1];
}

vkrndr::image_t& galileo::gbuffer_t::albedo_image()
{
    return gbuffer_.images[2];
}

VkExtent2D galileo::gbuffer_t::extent() const
{
    return vkrndr::to_2d_extent(gbuffer_.images[0].extent);
}

void galileo::gbuffer_t::resize(uint32_t const width, uint32_t const height)
{
    constexpr std::array formats{position_format, normal_format, albedo_format};

    destroy(&backend_->device(), &gbuffer_);
    gbuffer_ = ngngfx::create_gbuffer(backend_->device(),
        {.formats = formats,
            .extent = vkrndr::to_2d_extent(width, height),
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                VK_IMAGE_USAGE_SAMPLED_BIT,
            .allocation_flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            .required_memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT},
        VK_IMAGE_ASPECT_COLOR_BIT);
    object_name(backend_->device(), gbuffer_.images[0], "G-Buffer Position");
    object_name(backend_->device(), gbuffer_.images[1], "G-Buffer Normal");
    object_name(backend_->device(), gbuffer_.images[2], "G-Buffer Albedo");
}

void galileo::gbuffer_t::transition(VkCommandBuffer command_buffer,
    VkImageLayout const old_layout,
    VkPipelineStageFlags2 const src_stage_mask,
    VkAccessFlags2 const src_access_mask,
    VkImageLayout const new_layout,
    VkPipelineStageFlags2 const dst_stage_mask,
    VkAccessFlags2 const dst_access_mask) const
{
    std::array<VkImageMemoryBarrier2, 3> barriers; // NOLINT

    auto it{std::begin(barriers)}; // NOLINT(readability-qualified-auto)

    for (auto const& image : gbuffer_.images)
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
