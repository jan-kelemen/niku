#include <ngngfx_projection.hpp>

ngngfx::projection_t::projection_t(glm::vec2 near_far_planes,
    float aspect_ratio)
    : near_far_planes_{near_far_planes}
    , aspect_ratio_{aspect_ratio}
{
}

void ngngfx::projection_t::set_aspect_ratio(float aspect_ratio)
{
    aspect_ratio_ = aspect_ratio;
}

float ngngfx::projection_t::aspect_ratio() const { return aspect_ratio_; }

void ngngfx::projection_t::set_near_far_planes(glm::vec2 const near_far_planes)
{
    near_far_planes_ = near_far_planes;
}

glm::vec2 ngngfx::projection_t::near_far_planes() const
{
    return near_far_planes_;
}
