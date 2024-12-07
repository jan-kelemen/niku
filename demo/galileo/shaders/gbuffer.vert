#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "frame_info.glsl"
#include "render_graph.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;

layout(location = 0) out vec4 outColor;

void main() {
    gl_Position = 
        frame.projection * frame.view * graph.position[nonuniformEXT(gl_InstanceIndex)] * vec4(inPosition, 1.0);

    outColor = inColor * (gl_InstanceIndex + 1);
}

