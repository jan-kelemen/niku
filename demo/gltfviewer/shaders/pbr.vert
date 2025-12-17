#version 460

#extension GL_GOOGLE_include_directive : require

#include "environment.glsl" // (set = 0)

#include "scene_graph.glsl" // (set = 2)

layout(push_constant) uniform PushConsts
{
    uint debug;
    float iblFactor;
    uint modelIndex;
    uint materialIndex;
    VertexBuffer vertices;
    TransformBuffer transforms;
} pc;

#ifndef DEPTH_PASS
layout(location = 0) out vec3 outPosition;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec4 outTangent;
layout(location = 3) out vec4 outColor;
layout(location = 4) out vec2 outUV;
layout(location = 5) out vec4 outShadowPosition;
#endif

void main()
{
    const Vertex vert = unpackVertex(pc.vertices.v[gl_VertexIndex]);

    const Transform transform = pc.transforms.v[pc.modelIndex];

    vec4 worldPosition = transform.model * vec4(vert.position, 1.0);

#ifndef SHADOW_PASS
    gl_Position = env.projection * env.view * worldPosition;
#else
    for (uint i = 0; i != env.lightCount; ++i)
    {
        const Light light = env.lights[i];
        if (light.type != 1)
        {
            continue;
        }

        gl_Position = light.lightSpace * worldPosition;
        break;
    }
#endif

#ifndef DEPTH_PASS
    outPosition = worldPosition.xyz;
    outNormal = mat3(transform.normal) * normalize(vert.normal);
    outTangent = vec4(mat3(transform.normal) * vert.tangent.xyz, vert.tangent.w);
    outColor = vert.color;
    outUV = vert.uv;

    // clang-format off
    const mat4 bias = mat4(
        0.5, 0.0, 0.0, 0.0,
        0.0, 0.5, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.5, 0.5, 0.0, 1.0);
    // clang-format on

    for (uint i = 0; i != env.lightCount; ++i)
    {
        const Light light = env.lights[i];
        if (light.type != 1)
        {
            continue;
        }

        outShadowPosition = bias * light.lightSpace * worldPosition;
        break;
    }
#endif
}
