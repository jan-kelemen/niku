#ifndef EDITOR_EDITOR_APPLICATION_INCLUDED
#define EDITOR_EDITOR_APPLICATION_INCLUDED

#include <camera_controller.hpp>

#include <ngngfx_aircraft_camera.hpp>
#include <ngngfx_perspective_projection.hpp>

#include <ngnwsi_mouse.hpp>

#include <vkrndr_rendering_context.hpp>

#include <volk.h>

#include <memory>
#include <span>
#include <vector>

union SDL_Event;

namespace ngnwsi
{
    class render_window_t;
    class imgui_layer_t;
} // namespace ngnwsi

namespace vkrndr
{
    struct buffer_t;
} // namespace vkrndr

namespace editor
{
    struct grid_shader_t;
} // namespace editor

namespace editor
{
    class [[nodiscard]] application_t final
    {
    public:
        explicit application_t(std::span<char const*> command_line_parameters);

        application_t(application_t const&) = delete;

        application_t(application_t&&) noexcept = delete;

    public:
        ~application_t();

    public:
        [[nodiscard]] bool handle_event(SDL_Event const& event);

        [[nodiscard]] bool update();

    public:
        application_t& operator=(application_t const&) = delete;

        application_t& operator=(application_t&&) noexcept = delete;

    private:
        application_t();

    private:
        void process_command_line(std::span<char const*> const& parameters);

    private:
        std::unique_ptr<ngnwsi::render_window_t> main_window_;
        vkrndr::rendering_context_t rendering_context_;
        std::unique_ptr<ngnwsi::imgui_layer_t> imgui_;
        std::vector<VkCommandPool> pools_;
        std::vector<VkCommandBuffer> command_buffers_;

        VkDescriptorPool descriptor_pool_{VK_NULL_HANDLE};
        VkDescriptorSetLayout frame_info_layout_{VK_NULL_HANDLE};
        std::vector<VkDescriptorSet> frame_info_descriptors_;
        std::vector<vkrndr::buffer_t> frame_info_buffers_;

        std::unique_ptr<grid_shader_t> grid_shader_;

        ngnwsi::mouse_t mouse_;
        ngngfx::aircraft_camera_t camera_;
        ngngfx::perspective_projection_t projection_;
        camera_controller_t camera_controller_;
    };
} // namespace editor

#endif // !EDITOR_APPLICATION_INCLUDED
