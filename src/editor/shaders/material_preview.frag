#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(inUV, 0, 1);
}
