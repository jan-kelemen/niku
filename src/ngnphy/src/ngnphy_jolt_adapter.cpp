#include <ngnphy_jolt_adapter.hpp>

#include <glm/gtc/type_ptr.hpp>

#include <Jolt/Jolt.h> // IWYU pragma: keep
#include <Jolt/Math/Mat44.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Math/Vec4.h>

glm::vec3 ngnphy::to_glm(JPH::Vec3 const& v)
{
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    return glm::make_vec3(v.mF32);
    // NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
}

glm::vec4 ngnphy::to_glm(JPH::Vec4 const& v)
{
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    return glm::make_vec4(v.mF32);
    // NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
}

glm::mat4 ngnphy::to_glm(JPH::Mat44 const& v)
{
    return {to_glm(v.GetColumn4(0)),
        to_glm(v.GetColumn4(1)),
        to_glm(v.GetColumn4(2)),
        to_glm(v.GetColumn4(3))};
}
