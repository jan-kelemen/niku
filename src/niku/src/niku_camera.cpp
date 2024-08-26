#include <niku_camera.hpp>

niku::camera_t::camera_t() : camera_t({0.0f, 0.0f, 0.0f}, 16.0f / 9.0f) { }

niku::camera_t::camera_t(glm::vec3 const& position, float aspect_ratio)
    : position_{position}
    , aspect_ratio_{aspect_ratio}
{
}

float niku::camera_t::aspect_ratio() const { return aspect_ratio_; }

void niku::camera_t::set_aspect_ratio(float const aspect_ratio)
{
    aspect_ratio_ = aspect_ratio;
}

void niku::camera_t::set_position(glm::vec3 const& position)
{
    position_ = position;
}

glm::vec3 const& niku::camera_t::position() const { return position_; }
