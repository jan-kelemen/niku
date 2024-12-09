#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "frame_info.glsl"
#include "render_graph.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColor;

layout(location = 0) out vec3 outPosition;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec4 outColor;

void main() {
    const mat4 model = graph.position[nonuniformEXT(gl_InstanceIndex)];
    const vec4 worldPosition = model * vec4(inPosition, 1.0);

    gl_Position = frame.projection * frame.view * worldPosition;

    outPosition = worldPosition.xyz;
    outNormal = transpose(inverse(mat3(model))) * inNormal;
    outColor = inColor;
}

