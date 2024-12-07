#ifndef NGNPHY_JOLT_ADAPTER_INCLUDED
#define NGNPHY_JOLT_ADAPTER_INCLUDED

#include <glm/gtc/type_ptr.hpp>
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
    [[nodiscard]] constexpr glm::vec3 to_glm(JPH::Vec3 const& v)
    {
        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
        return glm::make_vec3(v.mF32);
        // NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    }

    [[nodiscard]] constexpr glm::vec4 to_glm(JPH::Vec4 const& v)
    {
        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
        return glm::make_vec4(v.mF32);
        // NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    }

    [[nodiscard]] constexpr glm::mat4 to_glm(JPH::Mat44 const& v)
    {
        return {to_glm(v.GetColumn4(0)),
            to_glm(v.GetColumn4(1)),
            to_glm(v.GetColumn4(2)),
            to_glm(v.GetColumn4(3))};
    }
} // namespace ngnphy

#endif
