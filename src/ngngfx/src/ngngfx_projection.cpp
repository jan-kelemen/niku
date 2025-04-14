#include <ngngfx_projection.hpp>

#include <ngngfx_camera.hpp>

ngngfx::projection_t::projection_t(camera_t const& camera,
    glm::vec2 near_far_planes,
    float aspect_ratio)
    : camera_{&camera}
    , near_far_planes_{near_far_planes}
    , aspect_ratio_{aspect_ratio}
{
}

glm::mat4 const& ngngfx::projection_t::view_matrix() const
{
    return camera_->view_matrix();
}

glm::vec3 const& ngngfx::projection_t::position() const
{
    return camera_->position();
}

void ngngfx::projection_t::set_aspect_ratio(float aspect_ratio)
{
    aspect_ratio_ = aspect_ratio;
}

float ngngfx::projection_t::aspect_ratio() const { return aspect_ratio_; }
