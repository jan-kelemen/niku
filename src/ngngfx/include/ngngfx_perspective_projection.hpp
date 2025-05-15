#ifndef NGNGFX_PERSPECTIVE_PROJECTION_INCLUDED
#define NGNGFX_PERSPECTIVE_PROJECTION_INCLUDED

#include <ngngfx_projection.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

namespace ngngfx
{
    class [[nodiscard]] perspective_projection_t : public projection_t
    {
    public:
        explicit perspective_projection_t(
            glm::vec2 near_far_planes = {0.1f, 1000.0f},
            float aspect_ratio = 16.0f / 9.0f,
            float fov = 45.0f,
            bool invert_y = true);

        perspective_projection_t(perspective_projection_t const&) = default;

        perspective_projection_t(perspective_projection_t&&) noexcept = default;

    public:
        ~perspective_projection_t() override = default;

    public:
        void set_aspect_ratio(float aspect_ratio);

        [[nodiscard]] float aspect_ratio() const;

    public: // projection_t overrides
        void update(glm::mat4 const& view_matrix) override;

        [[nodiscard]] glm::mat4 const& projection_matrix() const override;

        [[nodiscard]] glm::mat4 const& view_projection_matrix() const override;

    public:
        perspective_projection_t& operator=(
            perspective_projection_t const&) = default;

        perspective_projection_t& operator=(
            perspective_projection_t&&) noexcept = default;

    private:
        void calculate_view_projection_matrices(glm::mat4 const& view_matrix);

    protected:
        float fov_;
        float aspect_ratio_;

        glm::mat4 projection_matrix_;
        glm::mat4 view_projection_matrix_;
    };
} // namespace ngngfx

#endif
