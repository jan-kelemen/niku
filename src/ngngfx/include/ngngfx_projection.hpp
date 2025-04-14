#ifndef NGNGFX_PROJECTION_INCLUDED
#define NGNGFX_PROJECTION_INCLUDED

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace ngngfx
{
    class camera_t;
} // namespace ngngfx

namespace ngngfx
{
    class [[nodiscard]] projection_t
    {
    public:
        projection_t(camera_t const& camera,
            glm::vec2 near_far_planes,
            float aspect_ratio);

        projection_t(projection_t const&) = default;

        projection_t(projection_t&&) = default;

    public:
        virtual ~projection_t() = default;

    public:
        [[nodiscard]] glm::mat4 const& view_matrix() const;

        virtual void update() = 0;

        [[nodiscard]] virtual glm::mat4 const& projection_matrix() const = 0;

        [[nodiscard]] virtual glm::mat4 const&
        view_projection_matrix() const = 0;

    public:
        [[nodiscard]] glm::vec3 const& position() const;

        virtual void set_aspect_ratio(float aspect_ratio);

        [[nodiscard]] virtual float aspect_ratio() const;

    public:
        projection_t& operator=(projection_t const&) = default;

        projection_t& operator=(projection_t&&) noexcept = default;

    protected:
        camera_t const* camera_;
        glm::vec2 near_far_planes_;
        float aspect_ratio_;
    };
} // namespace ngngfx

#endif
