#ifndef NGNPHY_JOLT_ADAPTER_INCLUDED
#define NGNPHY_JOLT_ADAPTER_INCLUDED

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

// IWYU pragma: no_include <glm/detail/qualifier.hpp>

namespace JPH
{
    class Vec3;
    class Vec4;
    class Mat44;
} // namespace JPH

namespace ngnphy
{
    [[nodiscard]] glm::vec3 to_glm(JPH::Vec3 const& v);

    [[nodiscard]] glm::vec4 to_glm(JPH::Vec4 const& v);

    [[nodiscard]] glm::mat4 to_glm(JPH::Mat44 const& v);
} // namespace ngnphy

#endif
