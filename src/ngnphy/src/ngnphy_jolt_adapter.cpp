#include <ngnphy_jolt_adapter.hpp>

#include <glm/gtc/type_ptr.hpp>

// IWYU pragma: no_include <glm/detail/qualifier.hpp>

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

glm::quat ngnphy::to_glm(JPH::Quat const& v)
{
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    return glm::make_quat(v.mValue.mF32);
    // NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
}

JPH::Vec3 ngnphy::to_jolt(glm::vec3 const& v) { return {v.x, v.y, v.z}; }

JPH::Vec4 ngnphy::to_jolt(glm::vec4 const& v) { return {v.x, v.y, v.z, v.w}; }

JPH::Mat44 ngnphy::to_jolt(glm::mat4 const& v)
{
    return {to_jolt(v[0]), to_jolt(v[1]), to_jolt(v[2]), to_jolt(v[3])};
}

JPH::Quat ngnphy::to_jolt(glm::quat const& v) { return {v.x, v.y, v.z, v.w}; }
