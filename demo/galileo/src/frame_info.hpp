#ifndef GALILEO_FRAME_INFO_INCLUDED
#define GALILEO_FRAME_INFO_INCLUDED

#include <cppext_cycled_buffer.hpp>

#include <vkrndr_buffer.hpp>
#include <vkrndr_memory.hpp>

#include <volk.h>

#include <cstdint>

namespace ngnast
{
    struct bounding_box_t;
} // namespace ngnast

namespace ngngfx
{
    class projection_t;
} // namespace ngngfx

namespace vkrndr
{
    class backend_t;
} // namespace vkrndr

namespace galileo
{
    class [[nodiscard]] frame_info_t final
    {
    public:
        explicit frame_info_t(vkrndr::backend_t& backend);

        frame_info_t(frame_info_t const&) = delete;

        frame_info_t(frame_info_t&&) noexcept = delete;

    public:
        ~frame_info_t();

    public:
        [[nodiscard]] VkDescriptorSetLayout descriptor_set_layout() const;

        void begin_frame();

        void update(ngngfx::projection_t const& projection,
            uint32_t light_count);

        void bind_on(VkCommandBuffer command_buffer,
            VkPipelineLayout layout,
            VkPipelineBindPoint bind_point);

        void disperse_lights(ngnast::bounding_box_t const& bounding_box);

    public:
        frame_info_t& operator=(frame_info_t const&) = delete;

        frame_info_t& operator=(frame_info_t&&) noexcept = delete;

    private:
        struct [[nodiscard]] frame_data_t final
        {
            vkrndr::buffer_t info_buffer;
            vkrndr::mapped_memory_t info_map;

            vkrndr::buffer_t light_buffer;

            VkDescriptorSet descriptor_set{VK_NULL_HANDLE};
        };

    private:
        vkrndr::backend_t* backend_;

        VkDescriptorSetLayout descriptor_set_layout_{VK_NULL_HANDLE};

        cppext::cycled_buffer_t<frame_data_t> frame_data_;
    };
} // namespace galileo
#endif
