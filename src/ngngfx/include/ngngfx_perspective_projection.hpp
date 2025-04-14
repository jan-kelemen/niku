#ifndef NGNGFX_PERSPECTIVE_PROJECTION_INCLUDED
#define NGNGFX_PERSPECTIVE_PROJECTION_INCLUDED

#include <ngngfx_projection.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace ngngfx
{
    class camera_t;
} // namespace ngngfx

namespace ngngfx
{
    class [[nodiscard]] perspective_projection_t : public projection_t
    {
    public:
        perspective_projection_t(camera_t const& camera,
            glm::vec2 near_far_planes = {0.1f, 1000.0f},
            float aspect_ratio = 16.0f / 9.0f,
            float fov = 45.0f);

        perspective_projection_t(perspective_projection_t const&) = default;

        perspective_projection_t(perspective_projection_t&&) noexcept = default;

    public:
        ~perspective_projection_t() override = default;

    public: // projection_t overrides
        void update() override;

        [[nodiscard]] glm::mat4 const& projection_matrix() const override;

        [[nodiscard]] glm::mat4 const& view_projection_matrix() const override;

    public:
        perspective_projection_t& operator=(
            perspective_projection_t const&) = default;

        perspective_projection_t& operator=(
            perspective_projection_t&&) noexcept = default;

    private:
        void calculate_view_projection_matrices();

    protected:
        float fov_;

        glm::mat4 projection_matrix_;
        glm::mat4 view_projection_matrix_;
    };
} // namespace ngngfx

#endif
