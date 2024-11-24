#ifndef GLTFVIEWER_PYRAMID_BLUR_INCLUDED
#define GLTFVIEWER_PYRAMID_BLUR_INCLUDED

#include <cppext_cycled_buffer.hpp>

#include <vkrndr_image.hpp>
#include <vkrndr_pipeline.hpp>

#include <volk.h>

#include <cstdint>
#include <vector>

namespace vkrndr
{
    class backend_t;
} // namespace vkrndr

namespace gltfviewer
{
    class [[nodiscard]] pyramid_blur_t final
    {
    public:
        explicit pyramid_blur_t(vkrndr::backend_t& backend);

        pyramid_blur_t(pyramid_blur_t const&) = delete;

        pyramid_blur_t(pyramid_blur_t&&) noexcept = delete;

    public:
        ~pyramid_blur_t();

    public:
        [[nodiscard]] vkrndr::image_t source_image() const;

        void draw(uint32_t levels, VkCommandBuffer command_buffer);

        void resize(uint32_t width, uint32_t height);

    public:
        pyramid_blur_t& operator=(pyramid_blur_t const&) = delete;

        pyramid_blur_t& operator=(pyramid_blur_t&&) noexcept = delete;

    private:
        struct [[nodiscard]] frame_data_t final
        {
            VkDescriptorSet descriptor_{VK_NULL_HANDLE};
        };

    private:
        void downsample_pass(uint32_t levels, VkCommandBuffer command_buffer);

        void upsample_pass(uint32_t levels, VkCommandBuffer command_buffer);

        void create_downsample_resources();
        void create_upsample_resources();

    private:
        vkrndr::backend_t* backend_;

        VkSampler bilinear_sampler_;

        vkrndr::image_t pyramid_image_;
        std::vector<VkImageView> mip_views_;
        std::vector<VkExtent2D> mip_extents_;

        VkDescriptorSetLayout descriptor_layout_{VK_NULL_HANDLE};
        vkrndr::pipeline_t downsample_pipeline_;
        vkrndr::pipeline_t upsample_pipeline_;

        cppext::cycled_buffer_t<frame_data_t> frame_data_;
    };
} // namespace gltfviewer

#endif
