#ifndef GALILEO_APPLICATION_INCLUDED
#define GALILEO_APPLICATION_INCLUDED

#include <camera_controller.hpp>
#include <physics_engine.hpp>

#include <ngngfx_perspective_camera.hpp>

#include <ngnwsi_application.hpp>
#include <ngnwsi_mouse.hpp>

#include <vkglsl_guard.hpp>

#include <vkrndr_image.hpp>

#include <Jolt/Jolt.h> // IWYU pragma: keep
#include <Jolt/Physics/Body/BodyID.h> // IWYU pragma: keep

#include <SDL2/SDL_events.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

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
    class deferred_shader_t;
    class frame_info_t;
    class gbuffer_t;
    class gbuffer_shader_t;
    class materials_t;
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

        ngnwsi::mouse_t mouse_;
        ngngfx::perspective_camera_t camera_;
        camera_controller_t camera_controller_;

        physics_engine_t physics_engine_;
        std::vector<std::pair<size_t, JPH::BodyID>> bodies_;

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
        std::unique_ptr<physics_debug_t> physics_debug_;
    };
} // namespace galileo
#endif
