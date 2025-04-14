#include <ngngfx_camera.hpp>

ngngfx::camera_t::camera_t() : camera_t({0.0f, 0.0f, 0.0f}) { }

ngngfx::camera_t::camera_t(glm::vec3 const& position) : position_{position} { }

void ngngfx::camera_t::set_position(glm::vec3 const& position)
{
    position_ = position;
}

glm::vec3 const& ngngfx::camera_t::position() const { return position_; }
