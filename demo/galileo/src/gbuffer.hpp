#ifndef GALILEO_GBUFFER_INCLUDED
#define GALILEO_GBUFFER_INCLUDED

#include <ngngfx_gbuffer.hpp>

#include <vkrndr_image.hpp>

#include <volk.h>

#include <cstdint>

namespace vkrndr
{
    class backend_t;
} // namespace vkrndr

namespace galileo
{
    class [[nodiscard]] gbuffer_t final
    {
    public:
        static constexpr VkFormat position_format{
            VK_FORMAT_R16G16B16A16_SFLOAT};

        static constexpr VkFormat normal_format{VK_FORMAT_R16G16B16A16_SFLOAT};

        static constexpr VkFormat albedo_format{VK_FORMAT_R8G8B8A8_UNORM};

    public:
        explicit gbuffer_t(vkrndr::backend_t& backend);

        gbuffer_t(gbuffer_t const&) = delete;

        gbuffer_t(gbuffer_t&&) noexcept = delete;

    public:
        ~gbuffer_t();

    public:
        [[nodiscard]] vkrndr::image_t& position_image();

        [[nodiscard]] vkrndr::image_t& normal_image();

        [[nodiscard]] vkrndr::image_t& albedo_image();

        [[nodiscard]] VkExtent2D extent() const;

        void resize(uint32_t width, uint32_t height);

        void transition(VkCommandBuffer command_buffer,
            VkImageLayout old_layout,
            VkPipelineStageFlags2 src_stage_mask,
            VkAccessFlags2 src_access_mask,
            VkImageLayout new_layout,
            VkPipelineStageFlags2 dst_stage_mask,
            VkAccessFlags2 dst_access_mask) const;

    public:
        gbuffer_t& operator=(gbuffer_t const&) = delete;

        gbuffer_t& operator=(gbuffer_t&&) noexcept = delete;

    private:
        vkrndr::backend_t* backend_;

        ngngfx::gbuffer_t gbuffer_;
    };
} // namespace galileo

#endif
