#ifndef HEATX_APPLICATION_INCLUDED
#define HEATX_APPLICATION_INCLUDED

#include <ngnwsi_application.hpp>

#include <vkrndr_acceleration_structure.hpp>
#include <vkrndr_backend.hpp>
#include <vkrndr_buffer.hpp>

#include <SDL3/SDL_events.h>

#include <memory>

namespace ngnwsi
{
    class imgui_layer_t;
    class render_window_t;
} // namespace ngnwsi

namespace heatx
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

        void draw() override;

        void end_frame() override;

        void on_startup() override;

        void on_shutdown() override;

    private:
        void on_resize(uint32_t width, uint32_t height);

        void create_blas();

    private:
        vkrndr::rendering_context_t rendering_context_;
        std::unique_ptr<ngnwsi::render_window_t> render_window_;

        std::unique_ptr<vkrndr::backend_t> backend_;
        std::unique_ptr<ngnwsi::imgui_layer_t> imgui_;

        vkrndr::buffer_t vertex_buffer_;
        vkrndr::buffer_t index_buffer_;
        vkrndr::buffer_t transform_buffer_;

        vkrndr::acceleration_structure_t blas_;
    };
} // namespace heatx
#endif
