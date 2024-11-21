#ifndef GLTFVIEWER_PYRAMID_BLUR_INCLUDED
#define GLTFVIEWER_PYRAMID_BLUR_INCLUDED

#include <vkrndr_image.hpp>

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

        void resize(uint32_t width, uint32_t height);

    public:
        pyramid_blur_t& operator=(pyramid_blur_t const&) = delete;

        pyramid_blur_t& operator=(pyramid_blur_t&&) noexcept = delete;

    private:
        vkrndr::backend_t* backend_;

        vkrndr::image_t pyramid_image_;
        std::vector<VkImageView> mip_views_;
    };
} // namespace gltfviewer

#endif
