#version 460

#extension GL_GOOGLE_include_directive : require

#include "frame_info.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in float inSize;
layout(location = 2) in vec4 inColor;

layout(location = 0) out vec4 outColor;

void main()
{
    gl_Position = frame.projection * frame.view * vec4(inPosition, 1.0);

    gl_PointSize = inSize;

    outColor = inColor;
}
