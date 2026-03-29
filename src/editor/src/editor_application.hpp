#ifndef EDITOR_EDITOR_APPLICATION_INCLUDED
#define EDITOR_EDITOR_APPLICATION_INCLUDED

#include <camera_controller.hpp>

#include <cppext_thread_pool.hpp>

#include <ngngfx_aircraft_camera.hpp>
#include <ngngfx_perspective_projection.hpp>

#include <ngnwsi_fixed_timestep.hpp>
#include <ngnwsi_mouse.hpp>

#include <vkrndr_buffer.hpp> // IWYU pragma: keep
#include <vkrndr_rendering_context.hpp>

#include <entt/signal/delegate.hpp>
#include <entt/signal/dispatcher.hpp>
#include <entt/signal/fwd.hpp>

#include <volk.h>

#include <memory>
#include <shared_mutex>
#include <span>
#include <thread>
#include <vector>

union SDL_Event;

namespace ngnast::gltf
{
    class loader_t;
} // namespace ngnast::gltf

namespace ngnwsi
{
    class render_window_t;
    class imgui_layer_t;
} // namespace ngnwsi

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
        [[nodiscard]] entt::dispatcher& event_dispatcher();

        [[nodiscard]] bool update();

        void load_files(std::span<char const* const> const& file_list);

    public:
        application_t& operator=(application_t const&) = delete;

        application_t& operator=(application_t&&) noexcept = delete;

    private:
        struct [[nodiscard]] main_thread_callback_t final
        {
            entt::delegate<void(void)> callback;
        };

    private:
        application_t();

    private:
        void process_command_line(std::span<char const*> const& parameters);

        void handle_event(SDL_Event const& event);

        void execute_main_thread_callbacks(main_thread_callback_t& cb);

        void render();

    private:
        cppext::thread_pool_t thread_pool_;

        std::unique_ptr<ngnast::gltf::loader_t> gltf_loader_;

        entt::dispatcher event_dispatcher_;
        bool continue_running_{true};

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

        std::shared_mutex state_mutex_;
        ngnwsi::mouse_t mouse_;
        ngngfx::aircraft_camera_t camera_;
        ngngfx::perspective_projection_t projection_;
        camera_controller_t camera_controller_;

        ngnwsi::fixed_timestep_t timestep_;

        std::unique_ptr<std::jthread> render_thread_;
    };
} // namespace editor

inline entt::dispatcher& editor::application_t::event_dispatcher()
{
    return event_dispatcher_;
}

#endif // !EDITOR_APPLICATION_INCLUDED
