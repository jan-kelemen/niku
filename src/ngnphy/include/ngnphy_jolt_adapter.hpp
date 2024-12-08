#ifndef NGNPHY_JOLT_ADAPTER_INCLUDED
#define NGNPHY_JOLT_ADAPTER_INCLUDED

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <Jolt/Jolt.h> // IWYU pragma: keep
#include <Jolt/Math/Mat44.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Math/Vec4.h>

// IWYU pragma: no_include <glm/detail/qualifier.hpp>

namespace ngnphy
{
    [[nodiscard]] glm::vec3 to_glm(JPH::Vec3 const& v);

    [[nodiscard]] glm::vec4 to_glm(JPH::Vec4 const& v);

    [[nodiscard]] glm::mat4 to_glm(JPH::Mat44 const& v);

    [[nodiscard]] JPH::Vec3 to_jolt(glm::vec3 const& v);

    [[nodiscard]] JPH::Vec4 to_jolt(glm::vec4 const& v);

    [[nodiscard]] JPH::Mat44 to_jolt(glm::mat4 const& v);
} // namespace ngnphy

#endif
