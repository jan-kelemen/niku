#ifndef NGNGFX_ORTHOGRAPHIC_PROJECTION_INCLUDED
#define NGNGFX_ORTHOGRAPHIC_PROJECTION_INCLUDED

#include <ngngfx_projection.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

namespace ngngfx
{
    class [[nodiscard]] orthographic_projection_t : public projection_t
    {
    public:
        explicit orthographic_projection_t(
            glm::vec2 near_far_planes = {0.1f, 1000.0f},
            float aspect_ratio = 16.0f / 9.0f,
            glm::vec2 left_right = {-1.0f, 1.0f},
            glm::vec2 bottom_top = {-1.0f, 1.0f});

        orthographic_projection_t(orthographic_projection_t const&) = default;

        orthographic_projection_t(
            orthographic_projection_t&&) noexcept = default;

    public:
        ~orthographic_projection_t() override = default;

    public: // projection_t overrides
        void update(glm::mat4 const& view_matrix) override;

        [[nodiscard]] glm::mat4 const& projection_matrix() const override;

        [[nodiscard]] glm::mat4 const& view_projection_matrix() const override;

    public:
        orthographic_projection_t& operator=(
            orthographic_projection_t const&) = default;

        orthographic_projection_t& operator=(
            orthographic_projection_t&&) noexcept = default;

    private:
        void calculate_view_projection_matrices(glm::mat4 const& view_matrix);

    protected:
        glm::vec2 left_right_;
        glm::vec2 bottom_top_;

        glm::mat4 projection_matrix_;
        glm::mat4 view_projection_matrix_;
    };
} // namespace ngngfx

#endif
