#include <niku_camera.hpp>

niku::camera::camera() : camera({0.0f, 0.0f, 0.0f}, 16.0f / 9.0f) { }

niku::camera::camera(glm::vec3 const& position, float aspect_ratio)
    : position_{position}
    , aspect_ratio_{aspect_ratio}
{
}

float niku::camera::aspect_ratio() const { return aspect_ratio_; }

void niku::camera::set_aspect_ratio(float const aspect_ratio)
{
    aspect_ratio_ = aspect_ratio;
}

void niku::camera::set_position(glm::vec3 const& position)
{
    position_ = position;
}

glm::vec3 const& niku::camera::position() const { return position_; }
