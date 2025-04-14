#ifndef GALILEO_APPLICATION_INCLUDED
#define GALILEO_APPLICATION_INCLUDED

#include <camera_controller.hpp>
#include <follow_camera_controller.hpp>
#include <navmesh.hpp>
#include <physics_engine.hpp>
#include <world.hpp>

#include <cppext_thread_pool.hpp>

#include <ngnast_scene_model.hpp>

#include <ngngfx_aircraft_camera.hpp>
#include <ngngfx_perspective_projection.hpp>

#include <ngnscr_scripting_engine.hpp>

#include <ngnwsi_application.hpp>
#include <ngnwsi_mouse.hpp>

#include <vkglsl_guard.hpp>

#include <vkrndr_image.hpp>

#include <entt/entt.hpp>

#include <SDL3/SDL_events.h>

#include <cstdint>
#include <memory>
#include <random>

namespace ngnwsi
{
    class imgui_layer_t;
} // namespace ngnwsi

namespace vkrndr
{
    class backend_t;
} // namespace vkrndr

namespace galileo
{
    class batch_renderer_t;
    class character_t;
    class character_contact_listener_t;
    class world_contact_listener_t;
    class deferred_shader_t;
    class frame_info_t;
    class gbuffer_t;
    class gbuffer_shader_t;
    class materials_t;
    class navmesh_debug_t;
    class physics_debug_t;
    class postprocess_shader_t;
    class render_graph_t;
} // namespace galileo

namespace galileo
{
    class [[nodiscard]] application_t final : public ngnwsi::application_t
    {
    public:
        explicit application_t(bool debug);

        application_t(application_t const&) = delete;

        application_t(application_t&&) noexcept = delete;

    public:
        ~application_t() override;

    public:
        // cppcheck-suppress duplInheritedMember
        application_t& operator=(application_t const&) = delete;

        // cppcheck-suppress duplInheritedMember
        application_t& operator=(application_t&&) noexcept = delete;

    private: // ngnwsi::application callback interface
        bool handle_event(SDL_Event const& event, float delta_time) override;

        void update(float delta_time) override;

        bool begin_frame() override;

        void draw() override;

        void end_frame() override;

        void on_startup() override;

        void on_shutdown() override;

        void on_resize(uint32_t width, uint32_t height) override;

    private:
        void setup_world();

        void spawn_sphere();

        [[nodiscard]] bool is_spawner(uint32_t id) const;

        void stop_pathfinding(uint32_t id);

    private:
        vkglsl::guard_t glsl_guard_;

        ngnscr::scripting_engine_t scripting_engine_;
        cppext::thread_pool_t thread_pool_;

        ngnwsi::mouse_t mouse_;
        ngngfx::aircraft_camera_t camera_;
        ngngfx::perspective_projection_t projection_;

        entt::registry registry_;

        ngnast::primitive_t world_primitive_;
        ngnast::bounding_box_t world_aabb_;

        bool free_camera_active_{false};
        camera_controller_t free_camera_controller_;
        follow_camera_controller_t follow_camera_controller_;

        std::default_random_engine random_engine_;

        int light_count_{1};

        physics_engine_t physics_engine_;
        bool draw_physics_{true};

        entt::entity spawner_{entt::null};

        polymesh_parameters_t polymesh_params_;
        poly_mesh_t poly_mesh_;
        bool draw_main_polymesh_{true};
        bool draw_detail_polymesh_{true};
        bool draw_navigation_mesh_{true};
        bool draw_navigation_queries_{true};

        world_t world_;
        std::unique_ptr<world_contact_listener_t> world_listener_;

        std::unique_ptr<character_t> character_;
        std::unique_ptr<character_contact_listener_t> character_listener_;

        std::unique_ptr<vkrndr::backend_t> backend_;
        std::unique_ptr<ngnwsi::imgui_layer_t> imgui_;

        vkrndr::image_t depth_buffer_;
        std::unique_ptr<gbuffer_t> gbuffer_;
        vkrndr::image_t color_image_;

        std::unique_ptr<frame_info_t> frame_info_;
        std::unique_ptr<materials_t> materials_;
        std::unique_ptr<render_graph_t> render_graph_;
        std::unique_ptr<gbuffer_shader_t> gbuffer_shader_;
        std::unique_ptr<deferred_shader_t> deferred_shader_;
        std::unique_ptr<postprocess_shader_t> postprocess_shader_;
        std::unique_ptr<batch_renderer_t> batch_renderer_;
        std::unique_ptr<physics_debug_t> physics_debug_;
        std::unique_ptr<navmesh_debug_t> navmesh_debug_;
    };
} // namespace galileo
#endif
