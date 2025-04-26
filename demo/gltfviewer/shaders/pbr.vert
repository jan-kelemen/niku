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

#include "scene_graph.glsl" // (set = 2)

#ifndef DEPTH_PASS
layout(location = 0) out vec3 outPosition;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec4 outTangent;
layout(location = 3) out vec4 outColor;
layout(location = 4) out vec2 outUV;
#endif

void main()
{
    Transform transform = transforms.v[pc.modelIndex];

    vec4 worldPosition = transform.model * vec4(inPosition, 1.0);

#ifndef SHADOW_PASS
    gl_Position = env.projection * env.view * worldPosition;
#else
    const mat4 bias = mat4(
        0.5, 0.0, 0.0, 0.0,
        0.0, 0.5, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.5, 0.5, 0.0, 1.0);

    for (uint i = 0; i != env.lightCount; ++i)
    {
        const Light light = env.lights[i];
        if (light.type != 1)
        {
            continue;
        }

        gl_Position = bias * light.lightSpace * worldPosition;
        break;
    }
#endif

#ifndef DEPTH_PASS
    outPosition = worldPosition.xyz;
    outNormal = mat3(transform.normal) * normalize(inNormal);
    outTangent = vec4(mat3(transform.normal) * inTangent.xyz, inTangent.w);
    outColor = inColor;
    outUV = inUV;
#endif
}
