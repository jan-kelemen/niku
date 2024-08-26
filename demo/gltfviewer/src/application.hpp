#ifndef BEAM_APPLICATION_INCLUDED
#define BEAM_APPLICATION_INCLUDED

#include <niku_application.hpp>

#include <SDL2/SDL_events.h>

#include <memory>

namespace vkrndr
{
    class scene_t;
} // namespace vkrndr

namespace gltfviewer
{
    class scene_t;
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

        void update(float delta_time) override;

        [[nodiscard]] vkrndr::scene_t* render_scene() override;

    private:
        std::unique_ptr<scene_t> scene_;
    };
} // namespace beam
#endif
