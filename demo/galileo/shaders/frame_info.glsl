#ifndef GALILEO_FRAME_INFO_INCLUDED
#define GALILEO_FRAME_INFO_INCLUDED

layout(std430, set = 0, binding = 0) readonly buffer FrameInfo
{
    mat4 view;
    mat4 projection;
} frame;

#endif
