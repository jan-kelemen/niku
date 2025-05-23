#ifndef NGNGFX_PROJECTION_INCLUDED
#define NGNGFX_PROJECTION_INCLUDED

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

// IWYU pragma: no_include <glm/detail/qualifier.hpp>

namespace ngngfx
{
    class [[nodiscard]] projection_t
    {
    public:
        projection_t(glm::vec2 near_far_planes, bool invert_y);

        projection_t(projection_t const&) = default;

        projection_t(projection_t&&) = default;

    public:
        virtual ~projection_t() = default;

    public:
        virtual void update(glm::mat4 const& view_matrix) = 0;

        [[nodiscard]] virtual glm::mat4 const& projection_matrix() const = 0;

        [[nodiscard]] virtual glm::mat4 const&
        view_projection_matrix() const = 0;

    public:
        virtual void set_invert_y(bool value);

        [[nodiscard]] virtual bool invert_y() const;

        virtual void set_near_far_planes(glm::vec2 near_far_planes);

        [[nodiscard]] virtual glm::vec2 near_far_planes() const;

    public:
        projection_t& operator=(projection_t const&) = default;

        projection_t& operator=(projection_t&&) noexcept = default;

    protected:
        glm::vec2 near_far_planes_;
        bool invert_y_;
    };
} // namespace ngngfx

#endif
