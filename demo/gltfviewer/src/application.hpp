#ifndef BEAM_APPLICATION_INCLUDED
#define BEAM_APPLICATION_INCLUDED

#include <niku_application.hpp>

#include <vkrndr_image.hpp>

#include <SDL2/SDL_events.h>

#include <cstdint>
#include <memory>

namespace niku
{
    class imgui_layer_t;
}

namespace vkrndr
{
    class backend_t;
}

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

        void begin_frame() override;
        
        void update(float delta_time) override;

        bool begin_draw() override;

        void draw() override;

        void end_draw() override;

        void end_frame() override;

        void on_shutdown() override;

        void on_resize(uint32_t width, uint32_t height) override;

    private:
        std::unique_ptr<vkrndr::backend_t> backend_;
        std::unique_ptr<niku::imgui_layer_t> imgui_;

        vkrndr::image_t color_image_;
    };
} // namespace beam
#endif
