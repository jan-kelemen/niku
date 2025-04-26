#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "frame_info.glsl"
#include "scene_graph.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColor;
layout(location = 3) in vec2 inUV;
layout(location = 4) in uint inInstance;

layout(location = 0) out vec3 outPosition;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec4 outColor;
layout(location = 3) out vec2 outUV;
layout(location = 4) out uint outInstance;

void main() {
    const mat4 model = graph.nodes[inInstance].model;
    const vec4 worldPosition = model * vec4(inPosition, 1.0);

    gl_Position = frame.projection * frame.view * worldPosition;

    outPosition = worldPosition.xyz;
    outNormal = transpose(inverse(mat3(model))) * inNormal;
    outColor = inColor;
    outUV = inUV;
    outInstance = inInstance;
}

