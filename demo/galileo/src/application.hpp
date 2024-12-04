#ifndef GALILEO_APPLICATION_INCLUDED
#define GALILEO_APPLICATION_INCLUDED

#include <camera_controller.hpp>
#include <physics_engine.hpp>

#include <niku_application.hpp>
#include <niku_mouse.hpp>
#include <niku_perspective_camera.hpp>

#include <vkrndr_image.hpp>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>

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

namespace galileo
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

        void fixed_update(float delta_time) override;

        void update(float delta_time) override;

        bool begin_frame() override;

        void draw() override;

        void end_frame() override;

        void on_startup() override;

        void on_shutdown() override;

        void on_resize(uint32_t width, uint32_t height) override;

    private:
        niku::mouse_t mouse_;
        niku::perspective_camera_t camera_;

        physics_engine_t physics_engine_;
        std::vector<JPH::BodyID> bodies_;

        std::unique_ptr<vkrndr::backend_t> backend_;
        std::unique_ptr<niku::imgui_layer_t> imgui_;

        camera_controller_t camera_controller_;
    };
} // namespace galileo
#endif
