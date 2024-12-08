#include <ngngfx_camera.hpp>

ngngfx::camera_t::camera_t() : camera_t({0.0f, 0.0f, 0.0f}, 16.0f / 9.0f) { }

ngngfx::camera_t::camera_t(glm::vec3 const& position, float aspect_ratio)
    : position_{position}
    , aspect_ratio_{aspect_ratio}
{
}

float ngngfx::camera_t::aspect_ratio() const { return aspect_ratio_; }

void ngngfx::camera_t::set_aspect_ratio(float const aspect_ratio)
{
    aspect_ratio_ = aspect_ratio;
}

void ngngfx::camera_t::set_position(glm::vec3 const& position)
{
    position_ = position;
}

glm::vec3 const& ngngfx::camera_t::position() const { return position_; }
