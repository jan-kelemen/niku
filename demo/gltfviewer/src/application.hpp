#ifndef GLTFVIEWER_APPLICATION_INCLUDED
#define GLTFVIEWER_APPLICATION_INCLUDED

#include <camera_controller.hpp>
#include <model_selector.hpp>

#include <ngnast_gltf_loader.hpp>

#include <ngngfx_aircraft_camera.hpp>
#include <ngngfx_perspective_projection.hpp>

#include <ngnwsi_application.hpp>
#include <ngnwsi_mouse.hpp>

#include <vkglsl_guard.hpp>

#include <vkrndr_backend.hpp>
#include <vkrndr_image.hpp>

#include <SDL3/SDL_events.h>

#include <cstdint>
#include <memory>

namespace ngnwsi
{
    class imgui_layer_t;
    class render_window_t;
} // namespace ngnwsi

namespace gltfviewer
{
    class depth_pass_shader_t;
    class environment_t;
    class materials_t;
    class pbr_shader_t;
    class postprocess_shader_t;
    class pyramid_blur_t;
    class scene_graph_t;
    class resolve_shader_t;
    class shadow_map_t;
    class weighted_oit_shader_t;
    class weighted_blend_shader_t;
} // namespace gltfviewer

namespace gltfviewer
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
        bool should_run() override;

        bool handle_event(SDL_Event const& event) override;

        void update(float delta_time) override;

        void draw() override;

        void end_frame() override;

        void on_startup() override;

        void on_shutdown() override;

        void on_resize(uint32_t width, uint32_t height) override;

    private:
        void debug_draw();

    private:
        vkglsl::guard_t glsl_guard_;

        ngnwsi::mouse_t mouse_;
        ngngfx::aircraft_camera_t camera_;
        ngngfx::perspective_projection_t projection_;

        vkrndr::rendering_context_t rendering_context_;

        std::unique_ptr<ngnwsi::render_window_t> render_window_;
        std::unique_ptr<ngnwsi::imgui_layer_t> imgui_;

        std::unique_ptr<vkrndr::backend_t> backend_;
        vkrndr::image_t color_image_;
        vkrndr::image_t depth_buffer_;
        vkrndr::image_t resolve_image_;
        std::unique_ptr<environment_t> environment_;
        std::unique_ptr<materials_t> materials_;
        std::unique_ptr<scene_graph_t> scene_graph_;
        std::unique_ptr<depth_pass_shader_t> depth_pass_shader_;
        std::unique_ptr<shadow_map_t> shadow_map_;
        std::unique_ptr<pbr_shader_t> pbr_shader_;
        std::unique_ptr<weighted_oit_shader_t> weighted_oit_shader_;
        std::unique_ptr<resolve_shader_t> resolve_shader_;
        std::unique_ptr<pyramid_blur_t> pyramid_blur_;
        std::unique_ptr<weighted_blend_shader_t> weighted_blend_shader_;
        std::unique_ptr<postprocess_shader_t> postprocess_shader_;

        camera_controller_t camera_controller_;

        model_selector_t selector_;
        bool change_model_{false};
        ngnast::gltf::loader_t gltf_loader_;

        uint32_t debug_{0};
        float ibl_factor_{0.5f};
        uint32_t blur_levels_{4};
        float bloom_strength_{0.15f};
        bool color_conversion_{true};
        bool tone_mapping_{true};
        bool transparent_{true};
    };
} // namespace gltfviewer
#endif
