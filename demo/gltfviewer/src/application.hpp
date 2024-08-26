#ifndef BEAM_APPLICATION_INCLUDED
#define BEAM_APPLICATION_INCLUDED

#include <niku_application.hpp>

#include <SDL2/SDL_events.h>

#include <memory>

namespace vkrndr
{
    class scene;
} // namespace vkrndr

namespace gltfviewer
{
    class scene;
}

namespace gltfviewer
{
    class [[nodiscard]] application final : public niku::application
    {
    public:
        explicit application(bool debug);

        application(application const&) = delete;

        application(application&&) noexcept = delete;

    public:
        ~application() override;

    public:
        // cppcheck-suppress duplInheritedMember
        application& operator=(application const&) = delete;

        // cppcheck-suppress duplInheritedMember
        application& operator=(application&&) noexcept = delete;

    private: // niku::application callback interface
        bool handle_event(SDL_Event const& event) override;

        void update(float delta_time) override;

        [[nodiscard]] vkrndr::scene* render_scene() override;

    private:
        std::unique_ptr<scene> scene_;
    };
} // namespace beam
#endif
