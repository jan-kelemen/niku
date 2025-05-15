#include <ngngfx_projection.hpp>

ngngfx::projection_t::projection_t(glm::vec2 const near_far_planes,
    bool const invert_y)
    : near_far_planes_{near_far_planes}
    , invert_y_{invert_y}
{
}

void ngngfx::projection_t::set_invert_y(bool const value) { invert_y_ = value; }

bool ngngfx::projection_t::invert_y() const { return invert_y_; }

void ngngfx::projection_t::set_near_far_planes(glm::vec2 const near_far_planes)
{
    near_far_planes_ = near_far_planes;
}

glm::vec2 ngngfx::projection_t::near_far_planes() const
{
    return near_far_planes_;
}
