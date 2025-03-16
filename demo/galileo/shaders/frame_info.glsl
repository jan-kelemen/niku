#ifndef GALILEO_FRAME_INFO_INCLUDED
#define GALILEO_FRAME_INFO_INCLUDED

layout(std430, set = 0, binding = 0) restrict readonly buffer FrameInfo
{
    mat4 view;
    mat4 projection;
    vec3 position;
    uint lightCount;
} frame;

struct Light
{
    vec4 color;
    vec3 position;
};

layout(std430, set = 0, binding = 1) restrict readonly buffer Lights
{
    Light v[];
} lights;

#endif
