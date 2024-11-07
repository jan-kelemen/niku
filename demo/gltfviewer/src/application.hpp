#ifndef GLTFVIEWER_APPLICATION_INCLUDED
#define GLTFVIEWER_APPLICATION_INCLUDED

#include <camera_controller.hpp>
#include <model_selector.hpp>

#include <niku_application.hpp>
#include <niku_mouse.hpp>
#include <niku_perspective_camera.hpp>

#include <vkglsl_guard.hpp>
#include <vkgltf_loader.hpp>

#include <vkrndr_image.hpp>

#include <SDL2/SDL_events.h>

#include <cstdint>
#include <memory>

namespace niku
{
    class imgui_layer_t;
} // namespace niku

namespace vkrndr
{
    class backend_t;
} // namespace vkrndr

namespace gltfviewer
{
    class environment_t;
    class materials_t;
    class pbr_renderer_t;
    class postprocess_shader_t;
    class render_graph_t;
} // namespace gltfviewer

namespace gltfviewer
{
    class [[nodiscard]] application_t final : public niku::application_t
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

    private: // niku::application callback interface
        bool handle_event(SDL_Event const& event) override;

        void update(float delta_time) override;

        bool begin_frame() override;

        void draw() override;

        void end_frame() override;

        void on_startup() override;

        void on_shutdown() override;

        void on_resize(uint32_t width, uint32_t height) override;

    private:
        vkglsl::guard_t glsl_guard_;

        niku::mouse_t mouse_;
        niku::perspective_camera_t camera_;

        std::unique_ptr<vkrndr::backend_t> backend_;
        std::unique_ptr<niku::imgui_layer_t> imgui_;
        vkrndr::image_t color_image_;
        vkrndr::image_t depth_buffer_;
        std::unique_ptr<environment_t> environment_;
        std::unique_ptr<materials_t> materials_;
        std::unique_ptr<render_graph_t> render_graph_;
        std::unique_ptr<pbr_renderer_t> pbr_renderer_;
        std::unique_ptr<postprocess_shader_t> postprocess_shader_;

        camera_controller_t camera_controller_;

        model_selector_t selector_;
        vkgltf::loader_t gltf_loader_;

        uint32_t debug_{0};
        float ibl_factor_{0.5f};

        bool color_conversion_{true};
        bool tone_mapping_{true};
    };
} // namespace gltfviewer
#endif
