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
