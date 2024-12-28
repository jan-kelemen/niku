#version 460

#extension GL_GOOGLE_include_directive : require

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in vec4 inColor;
layout(location = 4) in vec2 inUV;

layout(push_constant) uniform PushConsts
{
    uint debug;
    float ibl_factor;
    uint modelIndex;
    uint materialIndex;
} pc;

#include "environment.glsl" // (set = 0)

struct Transform
{
    mat4 model;
    mat4 normal;
};

layout(std140, set = 2, binding = 0) readonly buffer TransformBuffer
{
    Transform v[];
}

transforms;

layout(location = 0) out vec3 outPosition;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec4 outTangent;
layout(location = 3) out vec4 outColor;
layout(location = 4) out vec2 outUV;

void main()
{
    vec4 worldPosition =
        transforms.v[pc.modelIndex].model * vec4(inPosition, 1.0);

    gl_Position = env.projection * env.view * worldPosition;

    outPosition = worldPosition.xyz;
    outNormal = mat3(transforms.v[pc.modelIndex].normal) * normalize(inNormal);
    outTangent = vec4(mat3(transforms.v[pc.modelIndex].normal) * inTangent.xyz,
        inTangent.w);
    outColor = inColor;
    outUV = inUV;
}
